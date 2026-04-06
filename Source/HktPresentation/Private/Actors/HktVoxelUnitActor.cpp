// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVoxelUnitActor.h"
#include "HktAnimInstance.h"
#include "HktPresentationState.h"
#include "HktPresentationLog.h"
#include "HktCoreProperties.h"
#include "HktVoxelSkinLayerAsset.h"
#include "Rendering/HktVoxelChunkComponent.h"
#include "Data/HktVoxelRenderCache.h"
#include "Data/HktVoxelTypes.h"
#include "Meshing/HktVoxelMeshScheduler.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"

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
	// 오프셋은 BeginPlay에서 Initialize() 이후에 설정 (Initialize가 덮어쓰기 때문)

	// 숨긴 스켈레톤 — 본-리지드 모드에서 본 트랜스폼 구동용
	HiddenSkeleton = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HiddenSkeleton"));
	HiddenSkeleton->SetupAttachment(RootScene);
	HiddenSkeleton->SetVisibility(false);
	HiddenSkeleton->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HiddenSkeleton->SetComponentTickEnabled(false);  // 본 모드 활성화 시 true로 전환
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

	// Initialize()가 오프셋을 (0,0,0)으로 리셋하므로 이후에 다시 설정
	// 캐릭터 중심(15.5 voxel) * 15 UU = -232.5 → 액터 원점(발 중앙)에 맞춤
	static constexpr float Offset = -15.5f * FHktVoxelChunk::VOXEL_SIZE;
	BodyChunk->SetRelativeLocation(FVector(Offset, Offset, 0.f));

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

	// [DEBUG] 파이프라인 추적 — 릴리스 전 제거
	if (EntityRenderCache)
	{
		FHktVoxelChunk* DbgChunk = EntityRenderCache->GetChunk(EntityChunkCoord);
		if (DbgChunk)
		{
			static int32 DbgFrame = 0;
			if (++DbgFrame <= 300 && DbgFrame % 30 == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[VoxelUnit] Frame=%d bMeshDirty=%d bMeshReady=%d OpaqueVerts=%d"),
					DbgFrame,
					(int32)DbgChunk->bMeshDirty.load(),
					(int32)DbgChunk->bMeshReady.load(),
					DbgChunk->OpaqueVertices.Num());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[VoxelUnit] Chunk not found in cache!"));
		}
	}
}

void AHktVoxelUnitActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownBoneChunks();
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

	// --- 본-리지드 애니메이션 포워딩 ---
	if (bBoneAnimatedMode)
	{
		UHktAnimInstance* HktAnim = GetAnimInstance();
		if (HktAnim)
		{
			if (bForceAll || Entity.bIsMoving.IsDirty(Frame))
				HktAnim->bIsMoving = Entity.bIsMoving.Get();

			if (bForceAll || Entity.Velocity.IsDirty(Frame))
			{
				FVector Vel = Entity.Velocity.Get();
				HktAnim->MoveSpeed = FVector2D(Vel.X, Vel.Y).Size();
				HktAnim->BlendSpaceX = HktAnim->MoveSpeed;
			}

			if (bForceAll || Entity.Stance.IsDirty(Frame))
				HktAnim->SyncStance(Entity.Stance.Get());

			if (bForceAll || Entity.MotionPlayRate.IsDirty(Frame) || Entity.AttackSpeed.IsDirty(Frame))
			{
				int32 RawRate = Entity.MotionPlayRate.Get();
				float SpeedScale = (RawRate > 0)
					? static_cast<float>(RawRate) / 100.0f
					: static_cast<float>(Entity.AttackSpeed.Get()) / 100.0f;
				if (SpeedScale <= 0.0f) SpeedScale = 1.0f;
				HktAnim->AttackPlayRate = SpeedScale;
			}

			if (bForceAll || Entity.CPRatio.IsDirty(Frame))
				HktAnim->CPRatio = Entity.CPRatio.Get();

			if (bForceAll || Entity.TagsDirtyFrame == Frame)
				HktAnim->SyncFromTagContainer(Entity.Tags);

			if (Entity.PendingAnimTriggers.Num() > 0)
			{
				for (const FGameplayTag& AnimTag : Entity.PendingAnimTriggers)
				{
					HktAnim->ApplyAnimTag(AnimTag);
				}
				const_cast<FHktEntityPresentation&>(Entity).PendingAnimTriggers.Reset();
			}
		}
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

	if (!EntityRenderCache)
	{
		return;
	}

	// 에셋에 본 데이터가 있으면 본-리지드 모드로 전환
	if (SkinAssembler.HasAnyBoneData())
	{
		// 첫 번째 본 에셋에서 BoneGroup 정보 추출
		TArray<FHktVoxelBoneGroup> AllBoneGroups;
		for (int32 i = 0; i < EHktVoxelSkinLayer::Count; i++)
		{
			const FHktVoxelSkinLayerData* LayerData = SkinAssembler.GetLayer(static_cast<EHktVoxelSkinLayer::Type>(i));
			if (LayerData && LayerData->VoxelLayerAsset.IsValid() && LayerData->VoxelLayerAsset->HasBoneData())
			{
				AllBoneGroups = LayerData->VoxelLayerAsset->BoneGroups;
				break;
			}
		}

		InitializeBoneChunks(AllBoneGroups);
	}
	else
	{
		// 정적 모드로 복귀
		if (bBoneAnimatedMode)
		{
			TeardownBoneChunks();
		}

		// 재조합 + 렌더 캐시 갱신 → dirty 마킹 → MeshScheduler가 비동기 메싱
		FHktVoxelChunk TempChunk;
		TempChunk.ChunkCoord = EntityChunkCoord;
		SkinAssembler.Assemble(TempChunk);

		const int32 VoxelCount = FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE;
		EntityRenderCache->LoadChunk(EntityChunkCoord, &TempChunk.Data[0][0][0], VoxelCount);
	}
}

void AHktVoxelUnitActor::OnPaletteChanged(uint8 NewPaletteRow)
{
	CachedPaletteRow = NewPaletteRow;

	// 팔레트 변경은 재메싱 불필요 — GPU에서 팔레트 텍스처 룩업으로 처리
	if (bBoneAnimatedMode)
	{
		for (auto& [BoneName, Comp] : BoneChunks)
		{
			if (Comp)
			{
				Comp->SetCustomPrimitiveDataFloat(0, static_cast<float>(NewPaletteRow));
			}
		}
	}
	else if (BodyChunk)
	{
		BodyChunk->SetCustomPrimitiveDataFloat(0, static_cast<float>(NewPaletteRow));
	}
}

void AHktVoxelUnitActor::PollMeshReady()
{
	if (!EntityRenderCache)
	{
		return;
	}

	if (bBoneAnimatedMode)
	{
		// 본-리지드 모드: 각 본 청크에 대해 메싱 완료 확인
		for (auto& [BoneName, ChunkCoord] : BoneChunkCoords)
		{
			FHktVoxelChunk* Chunk = EntityRenderCache->GetChunk(ChunkCoord);
			if (Chunk && Chunk->bMeshReady.load(std::memory_order_acquire))
			{
				Chunk->bMeshReady.store(false, std::memory_order_relaxed);
				if (auto* Comp = BoneChunks.Find(BoneName))
				{
					(*Comp)->OnMeshReady();
				}
			}
		}
	}
	else
	{
		// 정적 모드: 단일 BodyChunk
		if (!BodyChunk) return;

		FHktVoxelChunk* Chunk = EntityRenderCache->GetChunk(EntityChunkCoord);
		if (Chunk && Chunk->bMeshReady.load(std::memory_order_acquire))
		{
			Chunk->bMeshReady.store(false, std::memory_order_relaxed);
			BodyChunk->OnMeshReady();
		}
	}
}

// ============================================================================
// 본-리지드 애니메이션
// ============================================================================

UHktAnimInstance* AHktVoxelUnitActor::GetAnimInstance()
{
	if (!CachedAnimInstance.IsValid() && HiddenSkeleton)
	{
		CachedAnimInstance = Cast<UHktAnimInstance>(HiddenSkeleton->GetAnimInstance());
	}
	return CachedAnimInstance.Get();
}

void AHktVoxelUnitActor::InitializeBoneChunks(const TArray<FHktVoxelBoneGroup>& BoneGroups)
{
	// 기존 본 청크가 있으면 해제
	TeardownBoneChunks();

	if (BoneGroups.Num() == 0 || !EntityRenderCache)
	{
		return;
	}

	// 정적 BodyChunk 숨기기
	if (BodyChunk)
	{
		BodyChunk->SetVisibility(false);
	}

	// HiddenSkeleton 활성화
	if (HiddenSkeleton)
	{
		HiddenSkeleton->SetComponentTickEnabled(true);
	}

	// 메시 스케줄러를 다중 본 처리에 맞게 조정
	if (MeshScheduler)
	{
		MeshScheduler->SetMaxMeshPerFrame(4);
	}

	int32 BoneIndex = 0;
	for (const FHktVoxelBoneGroup& BoneGroup : BoneGroups)
	{
		if (BoneGroup.Voxels.Num() == 0)
		{
			continue;
		}

		// 고유 청크 좌표 할당 (공유 캐시에서 본 구분)
		const FIntVector ChunkCoord(BoneIndex + 1, 0, 0);  // 0은 EntityChunkCoord 예약

		// 청크 컴포넌트 생성
		UHktVoxelChunkComponent* BoneComp = NewObject<UHktVoxelChunkComponent>(this);
		BoneComp->RegisterComponent();

		// HiddenSkeleton의 본 소켓에 어태치
		BoneComp->AttachToComponent(HiddenSkeleton,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			BoneGroup.BoneName);

		// 렌더 캐시에 바인딩
		BoneComp->Initialize(EntityRenderCache.Get(), ChunkCoord);

		// 복셀 데이터를 임시 청크에 기록 → 캐시에 로드
		FHktVoxelChunk TempChunk;
		FMemory::Memzero(TempChunk.Data, sizeof(TempChunk.Data));
		TempChunk.ChunkCoord = ChunkCoord;
		UHktVoxelSkinLayerAsset::WriteBoneGroupToChunk(TempChunk, BoneGroup, CachedPaletteRow);

		const int32 VoxelCount = FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE;
		EntityRenderCache->LoadChunk(ChunkCoord, &TempChunk.Data[0][0][0], VoxelCount);

		// 복셀이 본 기준 올바른 위치에 나타나도록 오프셋 설정
		// 원본 32^3 공간에서의 복셀 월드 위치 - 본 레퍼런스 포즈 위치
		static constexpr float VoxelSize = FHktVoxelChunk::VOXEL_SIZE;
		static constexpr float HalfChunk = 15.5f * VoxelSize;
		const FVector VoxelOriginWorld = FVector(
			BoneGroup.LocalOrigin.X * VoxelSize - HalfChunk,
			BoneGroup.LocalOrigin.Y * VoxelSize - HalfChunk,
			BoneGroup.LocalOrigin.Z * VoxelSize);
		const FVector BoneOffset = VoxelOriginWorld - BoneGroup.RefPoseBonePos;
		BoneComp->SetRelativeLocation(BoneOffset);

		// 팔레트 설정
		BoneComp->SetCustomPrimitiveDataFloat(0, static_cast<float>(CachedPaletteRow));

		// 맵에 등록
		BoneChunks.Add(BoneGroup.BoneName, BoneComp);
		BoneChunkCoords.Add(BoneGroup.BoneName, ChunkCoord);

		BoneIndex++;
	}

	bBoneAnimatedMode = true;

	UE_LOG(LogTemp, Log, TEXT("[VoxelUnit] InitializeBoneChunks: %d bone chunks created"), BoneChunks.Num());
}

void AHktVoxelUnitActor::TeardownBoneChunks()
{
	for (auto& [BoneName, Comp] : BoneChunks)
	{
		if (Comp)
		{
			// 캐시에서 해당 청크 언로드
			if (auto* CoordPtr = BoneChunkCoords.Find(BoneName))
			{
				if (EntityRenderCache)
				{
					EntityRenderCache->UnloadChunk(*CoordPtr);
				}
			}
			Comp->DestroyComponent();
		}
	}

	BoneChunks.Empty();
	BoneChunkCoords.Empty();
	bBoneAnimatedMode = false;
	CachedAnimInstance.Reset();

	// 정적 모드 복원
	if (BodyChunk)
	{
		BodyChunk->SetVisibility(true);
	}
	if (HiddenSkeleton)
	{
		HiddenSkeleton->SetComponentTickEnabled(false);
	}
	if (MeshScheduler)
	{
		MeshScheduler->SetMaxMeshPerFrame(1);
	}
}
