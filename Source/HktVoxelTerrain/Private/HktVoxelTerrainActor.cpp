// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVoxelTerrainActor.h"
#include "HktVoxelTerrainStreamer.h"
#include "HktVoxelTerrainLog.h"
#include "Data/HktVoxelRenderCache.h"
#include "Data/HktVoxelTypes.h"
#include "Meshing/HktVoxelMeshScheduler.h"
#include "Rendering/HktVoxelChunkComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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

	PrewarmPool(InitialPoolSize);

	UE_LOG(LogHktVoxelTerrain, Log,
		TEXT("Terrain Actor initialized — ViewDistance=%.0f, Pool=%d, MaxLoad=%d/frame, MaxMesh=%d/frame"),
		ViewDistance, InitialPoolSize, MaxLoadsPerFrame, MaxMeshPerFrame);
}

void AHktVoxelTerrainActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 모든 활성 컴포넌트 해제
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

	TerrainMeshScheduler.Reset();
	TerrainCache.Reset();
	Streamer.Reset();

	Super::EndPlay(EndPlayReason);
}

void AHktVoxelTerrainActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!TerrainCache || !TerrainMeshScheduler || !Streamer)
	{
		return;
	}

	const FVector CameraPos = GetCameraWorldPos();

	// 1. 스트리밍 업데이트 — 로드/언로드 대상 계산
	Streamer->SetMaxLoadsPerFrame(MaxLoadsPerFrame);
	Streamer->SetHeightRange(HeightMinZ, HeightMaxZ);
	Streamer->UpdateStreaming(CameraPos, ViewDistance, ChunkWorldSize);

	// 2. 스트리밍 결과 반영
	ProcessStreamingResults();

	// 3. 메싱 스케줄링 (dirty 청크를 비동기 메싱)
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

	// 로드 — VM에서 데이터를 받아야 하지만, 아직 VM 연동 전이므로
	// 여기서는 LoadTerrainChunk()가 외부에서 호출된 후에만 컴포넌트를 할당한다.
	// 스트리머가 "로드할 청크" 목록을 제공하면, 외부(VM Bridge 등)에서
	// LoadTerrainChunk()를 호출해야 한다.
	//
	// 이미 RenderCache에 로드된 청크에 대해서만 컴포넌트 할당
	for (const FIntVector& Coord : Streamer->GetChunksToLoad())
	{
		if (ActiveChunks.Contains(Coord))
		{
			continue;
		}

		if (TerrainCache->GetChunk(Coord) != nullptr)
		{
			UHktVoxelChunkComponent* Comp = AcquireComponent();
			if (Comp)
			{
				Comp->Initialize(TerrainCache.Get(), Coord);
				if (TerrainMaterial)
				{
					Comp->SetVoxelMaterial(TerrainMaterial);
				}
				ActiveChunks.Add(Coord, Comp);
			}
		}
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

// === 외부 API ===

void AHktVoxelTerrainActor::LoadTerrainChunk(const FIntVector& ChunkCoord, const FHktVoxel* VoxelData, int32 VoxelCount)
{
	if (!TerrainCache)
	{
		return;
	}

	TerrainCache->LoadChunk(ChunkCoord, VoxelData, VoxelCount);

	// 이미 스트리밍 영역 내이면 컴포넌트 즉시 할당
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
