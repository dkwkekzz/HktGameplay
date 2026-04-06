// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVoxelTerrainActor.h"
#include "HktVoxelTerrainStreamer.h"
#include "HktVoxelTerrainLog.h"
#include "Data/HktVoxelRenderCache.h"
#include "Data/HktVoxelTypes.h"
#include "Meshing/HktVoxelMeshScheduler.h"
#include "Rendering/HktVoxelChunkComponent.h"
#include "Terrain/HktTerrainGenerator.h"
#include "Terrain/HktTerrainVoxel.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// FHktTerrainVoxel과 FHktVoxel은 동일 4바이트 레이아웃
static_assert(sizeof(FHktTerrainVoxel) == sizeof(FHktVoxel),
	"FHktTerrainVoxel and FHktVoxel must have identical size for safe reinterpret_cast");

AHktVoxelTerrainActor::AHktVoxelTerrainActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AHktVoxelTerrainActor::BeginPlay()
{
	Super::BeginPlay();

	// 테레인 전용 파이프라인 생성
	TerrainCache = MakeUnique<FHktVoxelRenderCache>();
	TerrainMeshScheduler = MakeUnique<FHktVoxelMeshScheduler>(TerrainCache.Get());
	TerrainMeshScheduler->SetMaxMeshPerFrame(MaxMeshPerFrame);

	Streamer = MakeUnique<FHktVoxelTerrainStreamer>();
	Streamer->SetMaxLoadsPerFrame(MaxLoadsPerFrame);
	Streamer->SetHeightRange(HeightMinZ, HeightMaxZ);
	Streamer->SetMaxLoadedChunks(MaxLoadedChunks);

	// 지형 생성기 초기화
	FHktTerrainGeneratorConfig GenConfig;
	GenConfig.Seed = TerrainSeed;
	GenConfig.HeightScale = HeightScale;
	GenConfig.HeightOffset = HeightOffset;
	GenConfig.WaterLevel = WaterLevel;
	GenConfig.MountainBlend = MountainBlend;
	GenConfig.bEnableCaves = bEnableCaves;
	Generator = MakeUnique<FHktTerrainGenerator>(GenConfig);

	PrewarmPool(InitialPoolSize);

	UE_LOG(LogHktVoxelTerrain, Log,
		TEXT("Terrain Actor initialized — Seed=%lld, ViewDist=%.0f, Pool=%d, MaxLoad=%d, MaxMesh=%d"),
		TerrainSeed, ViewDistance, InitialPoolSize, MaxLoadsPerFrame, MaxMeshPerFrame);
}

void AHktVoxelTerrainActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (auto& Pair : ActiveChunks)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	ActiveChunks.Empty();

	for (UHktVoxelChunkComponent* Comp : ComponentPool)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	ComponentPool.Empty();

	Generator.Reset();
	TerrainMeshScheduler.Reset();
	TerrainCache.Reset();
	Streamer.Reset();

	Super::EndPlay(EndPlayReason);
}

void AHktVoxelTerrainActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!TerrainCache || !TerrainMeshScheduler || !Streamer || !Generator)
	{
		return;
	}

	const FVector CameraPos = GetCameraWorldPos();

	// 1. 스트리밍 업데이트
	Streamer->SetMaxLoadsPerFrame(MaxLoadsPerFrame);
	Streamer->SetMaxLoadedChunks(MaxLoadedChunks);
	Streamer->SetHeightRange(HeightMinZ, HeightMaxZ);
	Streamer->UpdateStreaming(CameraPos, ViewDistance, ChunkWorldSize);

	// 2. 스트리밍 결과 반영 (생성 + 로드 + 컴포넌트 할당)
	ProcessStreamingResults();

	// 3. 메싱 스케줄링
	TerrainMeshScheduler->SetMaxMeshPerFrame(MaxMeshPerFrame);
	TerrainMeshScheduler->Tick(CameraPos);

	// 4. 메싱 완료 청크 → GPU 업로드
	ProcessMeshReadyChunks();
}

FVector AHktVoxelTerrainActor::GetCameraWorldPos() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);
			return ViewLoc;
		}
	}
	return FVector::ZeroVector;
}

void AHktVoxelTerrainActor::GenerateAndLoadChunk(const FIntVector& ChunkCoord)
{
	// 절차적 생성 (힙 할당 — 128KB는 워커 스레드 스택에 위험)
	constexpr int32 ChunkVoxelCount = 32 * 32 * 32;
	TArray<FHktTerrainVoxel> GeneratedVoxels;
	GeneratedVoxels.SetNumUninitialized(ChunkVoxelCount);
	Generator->GenerateChunk(ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z, GeneratedVoxels.GetData());

	// FHktTerrainVoxel → FHktVoxel (동일 4바이트 레이아웃)
	const FHktVoxel* VoxelData = reinterpret_cast<const FHktVoxel*>(GeneratedVoxels.GetData());
	TerrainCache->LoadChunk(ChunkCoord, VoxelData, ChunkVoxelCount);

	// 컴포넌트 할당
	UHktVoxelChunkComponent* Comp = AcquireComponent();
	if (Comp)
	{
		Comp->Initialize(TerrainCache.Get(), ChunkCoord);
		if (TerrainMaterial)
		{
			Comp->SetVoxelMaterial(TerrainMaterial);
		}
		ActiveChunks.Add(ChunkCoord, Comp);
	}
}

void AHktVoxelTerrainActor::ProcessStreamingResults()
{
	// 언로드
	for (const FIntVector& Coord : Streamer->GetChunksToUnload())
	{
		TerrainCache->UnloadChunk(Coord);

		if (UHktVoxelChunkComponent** Found = ActiveChunks.Find(Coord))
		{
			ReleaseComponent(*Found);
			ActiveChunks.Remove(Coord);
		}
	}

	// 로드: 스트리머가 요청한 청크를 절차적 생성 → RenderCache 로드 → 컴포넌트 할당
	for (const FIntVector& Coord : Streamer->GetChunksToLoad())
	{
		if (ActiveChunks.Contains(Coord))
		{
			continue;
		}

		GenerateAndLoadChunk(Coord);
	}
}

void AHktVoxelTerrainActor::ProcessMeshReadyChunks()
{
	for (auto& Pair : ActiveChunks)
	{
		const FHktVoxelChunk* Chunk = TerrainCache->GetChunk(Pair.Key);
		if (Chunk && Chunk->bMeshReady.load(std::memory_order_acquire))
		{
			Pair.Value->OnMeshReady();
		}
	}
}

// === 외부 API (VM 직접 연동용 — 절차적 생성 없이 데이터 주입) ===

void AHktVoxelTerrainActor::LoadTerrainChunk(const FIntVector& ChunkCoord, const FHktVoxel* VoxelData, int32 VoxelCount)
{
	if (!TerrainCache)
	{
		return;
	}

	TerrainCache->LoadChunk(ChunkCoord, VoxelData, VoxelCount);

	if (Streamer && Streamer->GetLoadedChunks().Contains(ChunkCoord) && !ActiveChunks.Contains(ChunkCoord))
	{
		UHktVoxelChunkComponent* Comp = AcquireComponent();
		if (Comp)
		{
			Comp->Initialize(TerrainCache.Get(), ChunkCoord);
			if (TerrainMaterial)
			{
				Comp->SetVoxelMaterial(TerrainMaterial);
			}
			ActiveChunks.Add(ChunkCoord, Comp);
		}
	}
}

void AHktVoxelTerrainActor::UnloadTerrainChunk(const FIntVector& ChunkCoord)
{
	if (!TerrainCache)
	{
		return;
	}

	TerrainCache->UnloadChunk(ChunkCoord);

	if (UHktVoxelChunkComponent** Found = ActiveChunks.Find(ChunkCoord))
	{
		ReleaseComponent(*Found);
		ActiveChunks.Remove(ChunkCoord);
	}
}

// === 컴포넌트 풀 ===

UHktVoxelChunkComponent* AHktVoxelTerrainActor::AcquireComponent()
{
	UHktVoxelChunkComponent* Comp = nullptr;

	if (ComponentPool.Num() > 0)
	{
		Comp = ComponentPool.Pop(EAllowShrinking::No);
	}
	else
	{
		Comp = NewObject<UHktVoxelChunkComponent>(this, NAME_None, RF_Transient);
		Comp->SetupAttachment(RootComponent);
		Comp->RegisterComponent();
	}

	if (Comp)
	{
		Comp->SetVisibility(true);
		Comp->SetComponentTickEnabled(false);
	}
	return Comp;
}

void AHktVoxelTerrainActor::ReleaseComponent(UHktVoxelChunkComponent* Comp)
{
	if (!Comp)
	{
		return;
	}

	Comp->SetVisibility(false);
	ComponentPool.Add(Comp);
}

void AHktVoxelTerrainActor::PrewarmPool(int32 Count)
{
	ComponentPool.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		UHktVoxelChunkComponent* Comp = NewObject<UHktVoxelChunkComponent>(this, NAME_None, RF_Transient);
		Comp->SetupAttachment(RootComponent);
		Comp->RegisterComponent();
		Comp->SetVisibility(false);
		ComponentPool.Add(Comp);
	}
}
