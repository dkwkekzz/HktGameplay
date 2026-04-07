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
	MeshScheduler.Reset();
}

AHktVoxelUnitActor::AHktVoxelUnitActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = RootScene;

	BodyChunk = CreateDefaultSubobject<UHktVoxelChunkComponent>(TEXT("BodyChunk"));
	BodyChunk->SetupAttachment(RootScene);

	// 숨긴 스켈레톤 — GPU 스키닝 모드에서 본 트랜스폼 구동용
	HiddenSkeleton = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HiddenSkeleton"));
	HiddenSkeleton->SetupAttachment(RootScene);
	HiddenSkeleton->SetVisibility(false);
	HiddenSkeleton->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HiddenSkeleton->SetComponentTickEnabled(false);
}

void AHktVoxelUnitActor::BeginPlay()
{
	Super::BeginPlay();

	EntityRenderCache = MakeShared<FHktVoxelRenderCache>();

	MeshScheduler = MakeUnique<FHktVoxelMeshScheduler>(EntityRenderCache.Get());
	MeshScheduler->SetMaxMeshPerFrame(1);

	BodyChunk->Initialize(EntityRenderCache.Get(), EntityChunkCoord);

	static constexpr float Offset = -15.5f * FHktVoxelChunk::VOXEL_SIZE;
	BodyChunk->SetRelativeLocation(FVector(Offset, Offset, 0.f));

	InitializeVoxelMesh();

	OnSkinSetChanged(0);
	OnPaletteChanged(0);
}

void AHktVoxelUnitActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	constexpr float InterpSpeed = 15.f;
	InterpLocation = FMath::VInterpTo(InterpLocation, CachedRenderLocation, DeltaTime, InterpSpeed);
	InterpRotation = FMath::RInterpTo(InterpRotation, CachedRotation, DeltaTime, InterpSpeed);

	SetActorLocationAndRotation(
		InterpLocation, InterpRotation,
		false, nullptr, ETeleportType::TeleportPhysics);

	if (MeshScheduler)
	{
		MeshScheduler->Tick(InterpLocation);
	}

	PollMeshReady();

	// GPU 스키닝: 매 프레임 본 트랜스폼 업데이트
	if (bGPUSkinningActive)
	{
		UpdateBoneTransformsFromSkeleton();
	}
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

	// --- 애니메이션 포워딩 (GPU 스키닝 모드) ---
	if (bGPUSkinningActive)
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
	if (bForceAll || Entity.VoxelSkinSet.IsDirty(Frame))
	{
		uint16 NewSkinSet = static_cast<uint16>(Entity.VoxelSkinSet.Get());
		if (NewSkinSet != CachedSkinSetID)
		{
			OnSkinSetChanged(NewSkinSet);
		}
	}

	if (bForceAll || Entity.VoxelPalette.IsDirty(Frame))
	{
		uint8 NewPalette = static_cast<uint8>(Entity.VoxelPalette.Get());
		if (NewPalette != CachedPaletteRow)
		{
			OnPaletteChanged(NewPalette);
		}
	}
}

UHktVoxelSkinLayerAsset* AHktVoxelUnitActor::GetDefaultAssetForLayer(EHktVoxelSkinLayer::Type Layer) const
{
	switch (Layer)
	{
	case EHktVoxelSkinLayer::Body:  return DefaultBodyAsset;
	case EHktVoxelSkinLayer::Head:  return DefaultHeadAsset;
	case EHktVoxelSkinLayer::Armor: return DefaultArmorAsset;
	default: return nullptr;
	}
}

void AHktVoxelUnitActor::InitializeVoxelMesh()
{
	if (!EntityRenderCache)
	{
		return;
	}

	// 에디터/블루프린트에서 지정한 에셋 연결
	auto SetupLayer = [this](EHktVoxelSkinLayer::Type Layer)
	{
		UHktVoxelSkinLayerAsset* Asset = GetDefaultAssetForLayer(Layer);
		if (!Asset) return;

		FHktVoxelSkinLayerData LayerData;
		LayerData.Layer = Layer;
		LayerData.SkinID.SkinSetID = CachedSkinSetID;
		LayerData.SkinID.PaletteRow = CachedPaletteRow;
		LayerData.bVisible = true;
		LayerData.VoxelLayerAsset = Asset;
		SkinAssembler.SetLayer(Layer, LayerData);
	};

	SetupLayer(EHktVoxelSkinLayer::Body);
	SetupLayer(EHktVoxelSkinLayer::Head);
	SetupLayer(EHktVoxelSkinLayer::Armor);

	// 에셋이 하나도 없으면 프로시저럴 폴백
	if (!SkinAssembler.GetLayer(EHktVoxelSkinLayer::Body))
	{
		FHktVoxelSkinLayerData BodyLayer;
		BodyLayer.Layer = EHktVoxelSkinLayer::Body;
		BodyLayer.SkinID.SkinSetID = CachedSkinSetID;
		BodyLayer.SkinID.PaletteRow = CachedPaletteRow;
		BodyLayer.bVisible = true;
		SkinAssembler.SetLayer(EHktVoxelSkinLayer::Body, BodyLayer);
	}

	// 스킨 조합 → 단일 청크
	FHktVoxelChunk TempChunk;
	TempChunk.ChunkCoord = EntityChunkCoord;
	SkinAssembler.Assemble(TempChunk);

	const int32 VoxelCount = FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE;
	EntityRenderCache->LoadChunk(EntityChunkCoord, &TempChunk.Data[0][0][0], VoxelCount);
}

void AHktVoxelUnitActor::OnSkinSetChanged(uint16 NewSkinSetID)
{
	CachedSkinSetID = NewSkinSetID;

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

	// 에셋에 본 데이터가 있으면 GPU 스키닝 모드로 전환
	if (SkinAssembler.HasAnyBoneData())
	{
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

		InitializeGPUSkinning(AllBoneGroups);
	}
	else
	{
		// 정적 모드
		bGPUSkinningActive = false;
		BoneNameToIndex.Empty();

		if (HiddenSkeleton)
		{
			HiddenSkeleton->SetComponentTickEnabled(false);
		}

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
		Chunk->bMeshReady.store(false, std::memory_order_relaxed);
		BodyChunk->OnMeshReady();
	}
}

// ============================================================================
// GPU 스키닝
// ============================================================================

UHktAnimInstance* AHktVoxelUnitActor::GetAnimInstance()
{
	if (!CachedAnimInstance.IsValid() && HiddenSkeleton)
	{
		CachedAnimInstance = Cast<UHktAnimInstance>(HiddenSkeleton->GetAnimInstance());
	}
	return CachedAnimInstance.Get();
}

void AHktVoxelUnitActor::InitializeGPUSkinning(const TArray<FHktVoxelBoneGroup>& BoneGroups)
{
	if (BoneGroups.Num() == 0 || !EntityRenderCache)
	{
		return;
	}

	// HiddenSkeleton에 SkeletalMesh 설정
	if (HiddenSkeleton && !HiddenSkeleton->GetSkeletalMeshAsset())
	{
		for (int32 i = 0; i < EHktVoxelSkinLayer::Count; i++)
		{
			const FHktVoxelSkinLayerData* LayerData = SkinAssembler.GetLayer(static_cast<EHktVoxelSkinLayer::Type>(i));
			if (LayerData && LayerData->VoxelLayerAsset.IsValid() && LayerData->VoxelLayerAsset->HasBoneData())
			{
				USkeletalMesh* SkelMesh = LayerData->VoxelLayerAsset->SourceMesh.LoadSynchronous();
				if (SkelMesh)
				{
					HiddenSkeleton->SetSkeletalMeshAsset(SkelMesh);
					break;
				}
			}
		}
	}

	if (HiddenSkeleton)
	{
		HiddenSkeleton->SetComponentTickEnabled(true);
	}

	// 본 이름 → 인덱스 매핑 구축 (인덱스 0 = identity/루트, 유효 본은 1~)
	BoneNameToIndex.Empty();
	uint8 NextBoneIndex = 1;
	for (const FHktVoxelBoneGroup& BoneGroup : BoneGroups)
	{
		if (BoneGroup.Voxels.Num() > 0 && !BoneNameToIndex.Contains(BoneGroup.BoneName))
		{
			BoneNameToIndex.Add(BoneGroup.BoneName, NextBoneIndex);
			NextBoneIndex++;
			if (NextBoneIndex >= 128) break;  // 7비트 한계
		}
	}

	// 복셀 조합 + 본 인덱스 맵을 단일 청크에 기록
	FHktVoxelChunk TempChunk;
	TempChunk.ChunkCoord = EntityChunkCoord;
	FMemory::Memzero(TempChunk.Data, sizeof(TempChunk.Data));
	TempChunk.AllocBoneIndices();

	// 에셋의 SparseVoxels를 청크에 기록 (모든 레이어 조합)
	SkinAssembler.Assemble(TempChunk);

	// 본 인덱스 맵 기록 — BoneGroups에서 각 복셀 위치에 본 인덱스 할당
	for (const FHktVoxelBoneGroup& BoneGroup : BoneGroups)
	{
		const uint8* BoneIdxPtr = BoneNameToIndex.Find(BoneGroup.BoneName);
		if (!BoneIdxPtr) continue;
		const uint8 BoneIdx = *BoneIdxPtr;

		for (const FHktVoxelSparse& V : BoneGroup.Voxels)
		{
			// BoneGroup의 로컬 좌표는 원본 32^3 공간 기준
			if (V.X < FHktVoxelChunk::SIZE && V.Y < FHktVoxelChunk::SIZE && V.Z < FHktVoxelChunk::SIZE)
			{
				TempChunk.SetBoneIndex(V.X, V.Y, V.Z, BoneIdx);
			}
		}
	}

	// 캐시에 로드 (BoneIndices 포함 — 메싱 시 버텍스에 패킹됨)
	const int32 VoxelCount = FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE;

	// LoadChunk는 Data만 복사하므로, BoneIndices는 별도로 전달해야 한다.
	// 직접 청크에 접근하여 BoneIndices를 설정
	EntityRenderCache->LoadChunk(EntityChunkCoord, &TempChunk.Data[0][0][0], VoxelCount);

	// 캐시의 청크에 본 인덱스 맵 전달
	FHktVoxelChunk* CachedChunk = EntityRenderCache->GetChunk(EntityChunkCoord);
	if (CachedChunk && TempChunk.BoneIndices)
	{
		CachedChunk->AllocBoneIndices();
		FMemory::Memcpy(CachedChunk->BoneIndices.Get(), TempChunk.BoneIndices.Get(),
			FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE);
		// dirty 마킹하여 본 인덱스 포함 재메싱 트리거
		CachedChunk->bMeshDirty.store(true, std::memory_order_release);
	}

	bGPUSkinningActive = true;

	UE_LOG(LogTemp, Log, TEXT("[VoxelUnit] GPU Skinning initialized: %d bones"), BoneNameToIndex.Num());
}

void AHktVoxelUnitActor::UpdateBoneTransformsFromSkeleton()
{
	if (!HiddenSkeleton || !HiddenSkeleton->GetSkeletalMeshAsset() || BoneNameToIndex.Num() == 0)
	{
		return;
	}

	// 본 트랜스폼 배열 구성: float4 × 3 per bone (3x4 affine matrix)
	// 인덱스 0 = identity, 유효 본 인덱스는 1~
	const int32 NumBones = BoneNameToIndex.Num() + 1;  // +1 for index 0
	TArray<FVector4f> BoneMatrixRows;
	BoneMatrixRows.SetNumZeroed(NumBones * 3);

	// 인덱스 0 = identity matrix
	BoneMatrixRows[0] = FVector4f(1, 0, 0, 0);
	BoneMatrixRows[1] = FVector4f(0, 1, 0, 0);
	BoneMatrixRows[2] = FVector4f(0, 0, 1, 0);

	const TArray<FTransform>& SpaceBases = HiddenSkeleton->GetComponentSpaceTransforms();

	for (const auto& [BoneName, BoneIdx] : BoneNameToIndex)
	{
		const int32 SkeletonBoneIndex = HiddenSkeleton->GetBoneIndex(BoneName);
		if (SkeletonBoneIndex == INDEX_NONE || SkeletonBoneIndex >= SpaceBases.Num())
		{
			// Identity 폴백
			const int32 Base = BoneIdx * 3;
			BoneMatrixRows[Base + 0] = FVector4f(1, 0, 0, 0);
			BoneMatrixRows[Base + 1] = FVector4f(0, 1, 0, 0);
			BoneMatrixRows[Base + 2] = FVector4f(0, 0, 1, 0);
			continue;
		}

		// Component-space 본 트랜스폼 → 3x4 행렬
		const FMatrix44f BoneMatrix = FMatrix44f(SpaceBases[SkeletonBoneIndex].ToMatrixWithScale());
		const int32 Base = BoneIdx * 3;
		BoneMatrixRows[Base + 0] = FVector4f(BoneMatrix.M[0][0], BoneMatrix.M[0][1], BoneMatrix.M[0][2], BoneMatrix.M[0][3]);
		BoneMatrixRows[Base + 1] = FVector4f(BoneMatrix.M[1][0], BoneMatrix.M[1][1], BoneMatrix.M[1][2], BoneMatrix.M[1][3]);
		BoneMatrixRows[Base + 2] = FVector4f(BoneMatrix.M[2][0], BoneMatrix.M[2][1], BoneMatrix.M[2][2], BoneMatrix.M[2][3]);
	}

	BodyChunk->UpdateBoneTransforms(BoneMatrixRows);
}
