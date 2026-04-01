// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVoxelUnitActor.h"
#include "HktPresentationState.h"
#include "HktPresentationLog.h"
#include "HktCoreProperties.h"
#include "Rendering/HktVoxelChunkComponent.h"
#include "Data/HktVoxelRenderCache.h"
#include "Data/HktVoxelTypes.h"
#include "Meshing/HktVoxelMeshScheduler.h"

const FIntVector AHktVoxelUnitActor::EntityChunkCoord = FIntVector::ZeroValue;

AHktVoxelUnitActor::~AHktVoxelUnitActor()
{
	// TUniquePtr<FHktVoxelMeshScheduler> 소멸을 위해 명시적 정의 (complete type 필요)
	MeshScheduler.Reset();
}

AHktVoxelUnitActor::AHktVoxelUnitActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = RootScene;

	BodyChunk = CreateDefaultSubobject<UHktVoxelChunkComponent>(TEXT("BodyChunk"));
	BodyChunk->SetupAttachment(RootScene);
	// 복셀 캐릭터의 원점은 발 위치 — 청크를 아래로 오프셋하여 중심 맞춤
	BodyChunk->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
}

void AHktVoxelUnitActor::BeginPlay()
{
	Super::BeginPlay();

	// 엔티티 전용 렌더 캐시 생성 (월드 복셀과 독립)
	EntityRenderCache = MakeShared<FHktVoxelRenderCache>();

	// 메싱 스케줄러 생성
	MeshScheduler = MakeUnique<FHktVoxelMeshScheduler>(EntityRenderCache.Get());
	MeshScheduler->SetMaxMeshPerFrame(1);  // 엔티티 복셀은 1청크뿐

	// 청크 컴포넌트를 렌더 캐시에 바인딩
	BodyChunk->Initialize(EntityRenderCache.Get(), EntityChunkCoord);

	// 초기 복셀 메시 로드
	InitializeVoxelMesh();

	OnSkinSetChanged(0);   // SkinSetID=0, GenerateDefaultShape() 실행됨
	OnPaletteChanged(0);   // 팔레트 0번 행 (기본 흰색)
}

void AHktVoxelUnitActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 위치 보간 (기존 AHktUnitActor와 동일한 패턴)
	constexpr float InterpSpeed = 15.f;
	InterpLocation = FMath::VInterpTo(InterpLocation, CachedRenderLocation, DeltaTime, InterpSpeed);
	InterpRotation = FMath::RInterpTo(InterpRotation, CachedRotation, DeltaTime, InterpSpeed);

	SetActorLocationAndRotation(
		InterpLocation, InterpRotation,
		false, nullptr, ETeleportType::TeleportPhysics);

	// 메싱 스케줄링 (dirty 청크가 있으면 비동기 메싱)
	if (MeshScheduler)
	{
		MeshScheduler->Tick(InterpLocation);
	}

	// 메싱 완료 시 GPU 업로드
	PollMeshReady();
}

void AHktVoxelUnitActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MeshScheduler.Reset();
	EntityRenderCache.Reset();
	Super::EndPlay(EndPlayReason);
}

void AHktVoxelUnitActor::ApplyPresentation(
	const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll,
	TFunctionRef<AActor*(FHktEntityId)> /*GetActorFunc*/)
{
	// --- Transform ---
	CachedRenderLocation = Entity.RenderLocation.Get();
	CachedRotation = Entity.Rotation.Get();

	if (bForceAll)
	{
		InterpLocation = CachedRenderLocation;
		InterpRotation = CachedRotation;
	}

	// --- 복셀 스킨 변경 감지 ---
	// VoxelSkinSet 변경 → 전체 재조합 + 재메싱 (레이어 교체)
	if (bForceAll || Entity.VoxelSkinSet.IsDirty(Frame))
	{
		uint16 NewSkinSet = static_cast<uint16>(Entity.VoxelSkinSet.Get());
		if (NewSkinSet != CachedSkinSetID)
		{
			OnSkinSetChanged(NewSkinSet);
		}
	}

	// VoxelPalette 변경 → 팔레트만 교체 (재메싱 불필요, GPU 파라미터만 변경)
	if (bForceAll || Entity.VoxelPalette.IsDirty(Frame))
	{
		uint8 NewPalette = static_cast<uint8>(Entity.VoxelPalette.Get());
		if (NewPalette != CachedPaletteRow)
		{
			OnPaletteChanged(NewPalette);
		}
	}
}

void AHktVoxelUnitActor::InitializeVoxelMesh()
{
	if (!EntityRenderCache)
	{
		return;
	}

	// 기본 스킨으로 Body 레이어 설정
	FHktVoxelSkinLayerData BodyLayer;
	BodyLayer.Layer = EHktVoxelSkinLayer::Body;
	BodyLayer.SkinID.SkinSetID = CachedSkinSetID;
	BodyLayer.SkinID.PaletteRow = CachedPaletteRow;
	BodyLayer.bVisible = true;
	SkinAssembler.SetLayer(EHktVoxelSkinLayer::Body, BodyLayer);

	// 스킨 조합 → 청크 데이터 생성
	FHktVoxelChunk TempChunk;
	TempChunk.ChunkCoord = EntityChunkCoord;
	SkinAssembler.Assemble(TempChunk);

	// 렌더 캐시에 청크 로드 (LoadChunk가 dirty 마킹)
	const int32 VoxelCount = FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE;
	EntityRenderCache->LoadChunk(EntityChunkCoord, &TempChunk.Data[0][0][0], VoxelCount);
}

void AHktVoxelUnitActor::OnSkinSetChanged(uint16 NewSkinSetID)
{
	CachedSkinSetID = NewSkinSetID;

	// 모든 레이어의 SkinSetID 갱신
	for (int32 i = 0; i < EHktVoxelSkinLayer::Count; i++)
	{
		auto Layer = static_cast<EHktVoxelSkinLayer::Type>(i);
		const FHktVoxelSkinLayerData* Existing = SkinAssembler.GetLayer(Layer);
		if (Existing)
		{
			FHktVoxelSkinLayerData Updated = *Existing;
			Updated.SkinID.SkinSetID = NewSkinSetID;
			SkinAssembler.SetLayer(Layer, Updated);
		}
	}

	// 재조합 + 렌더 캐시 갱신 → dirty 마킹 → MeshScheduler가 비동기 메싱
	FHktVoxelChunk TempChunk;
	TempChunk.ChunkCoord = EntityChunkCoord;
	SkinAssembler.Assemble(TempChunk);

	const int32 VoxelCount = FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE;
	EntityRenderCache->LoadChunk(EntityChunkCoord, &TempChunk.Data[0][0][0], VoxelCount);
}

void AHktVoxelUnitActor::OnPaletteChanged(uint8 NewPaletteRow)
{
	CachedPaletteRow = NewPaletteRow;

	// 팔레트 변경은 재메싱 불필요 — GPU에서 팔레트 텍스처 룩업으로 처리
	// CustomPrimitiveData로 팔레트 행 번호를 전달하면 셰이더가 읽음
	if (BodyChunk)
	{
		BodyChunk->SetCustomPrimitiveDataFloat(0, static_cast<float>(NewPaletteRow));
	}
}

void AHktVoxelUnitActor::PollMeshReady()
{
	if (!EntityRenderCache || !BodyChunk)
	{
		return;
	}

	FHktVoxelChunk* Chunk = EntityRenderCache->GetChunk(EntityChunkCoord);
	if (Chunk && Chunk->bMeshReady.load(std::memory_order_acquire))
	{
		// 소비 — 다음 프레임에 중복 업로드 방지
		Chunk->bMeshReady.store(false, std::memory_order_relaxed);
		BodyChunk->OnMeshReady();
	}
}
