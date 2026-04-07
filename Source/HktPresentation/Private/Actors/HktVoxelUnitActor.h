// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HktSelectable.h"
#include "IHktPresentableActor.h"
#include "HktVoxelSkinTypes.h"
#include "HktVoxelSkinAssembler.h"
#include "Meshing/HktVoxelMeshScheduler.h"
#include "HktVoxelUnitActor.generated.h"

class UHktVoxelChunkComponent;
class USkeletalMeshComponent;
class UHktAnimInstance;
class UHktVoxelSkinLayerAsset;
class FHktVoxelRenderCache;
struct FHktEntityPresentation;
struct FHktVoxelBoneGroup;

/**
 * 복셀 캐릭터/유닛용 Actor.
 *
 * 기존 AHktUnitActor(SkeletalMesh 기반)의 복셀 버전.
 * 단일 UHktVoxelChunkComponent + GPU 스키닝으로 스켈레톤 애니메이션을 구현한다.
 * HiddenSkeleton이 본 트랜스폼을 구동하고, 셰이더에서 버텍스별 본 트랜스폼을 적용.
 *
 * 스킨 조합: FHktVoxelSkinAssembler로 7레이어(Body~Weapon) 조합
 * 팔레트 교체: 재메싱 없이 PaletteRow 변경만으로 색상 즉시 전환
 * 장비 교체: 레이어 교체 → Assemble → 재메싱 (비동기)
 *
 * VM 프로퍼티 매핑:
 *   VoxelSkinSet   → 스킨 세트 ID (외형 메시 결정)
 *   VoxelPalette   → 팔레트 행 번호 (색상 결정, 재메싱 불필요)
 */
UCLASS(Blueprintable)
class AHktVoxelUnitActor : public AActor, public IHktSelectable, public IHktPresentableActor
{
	GENERATED_BODY()

public:
	AHktVoxelUnitActor();
	virtual ~AHktVoxelUnitActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IHktSelectable
	virtual FHktEntityId GetEntityId() const override { return CachedEntityId; }

	// IHktPresentableActor
	virtual void SetEntityId(FHktEntityId InEntityId) override { CachedEntityId = InEntityId; }
	virtual void ApplyTransform(const FHktEntityPresentation& Entity) override {}
	virtual void ApplyPresentation(const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll,
		TFunctionRef<AActor*(FHktEntityId)> GetActorFunc) override;

private:
	/** 초기 복셀 데이터 로드 + 메싱 요청 */
	void InitializeVoxelMesh();

	/** 스킨 세트 변경 시 → 전체 재조합 + 재메싱 */
	void OnSkinSetChanged(uint16 NewSkinSetID);

	/** 팔레트 변경 시 → 머티리얼 파라미터만 변경 (재메싱 불필요) */
	void OnPaletteChanged(uint8 NewPaletteRow);

	/** 메싱 완료 콜백 → GPU 업로드 */
	void PollMeshReady();

	/** GPU 스키닝 모드 초기화 — 본 인덱스 맵을 청크에 기록 */
	void InitializeGPUSkinning(const TArray<FHktVoxelBoneGroup>& BoneGroups);

	/** 매 프레임 본 트랜스폼을 BodyChunk에 전달 */
	void UpdateBoneTransformsFromSkeleton();

	/** HiddenSkeleton의 AnimInstance 캐시 반환 */
	UHktAnimInstance* GetAnimInstance();

	// --- Components ---
	UPROPERTY(VisibleAnywhere, Category = "HKT|Voxel")
	TObjectPtr<USceneComponent> RootScene;

	/** 단일 복셀 청크 — 정적 모드와 GPU 스키닝 모드 모두 사용 */
	UPROPERTY(VisibleAnywhere, Category = "HKT|Voxel")
	TObjectPtr<UHktVoxelChunkComponent> BodyChunk;

	/** 숨긴 스켈레톤 — 본 트랜스폼 구동용 (렌더링 안 함) */
	UPROPERTY(VisibleAnywhere, Category = "HKT|Voxel")
	TObjectPtr<USkeletalMeshComponent> HiddenSkeleton;

	/** GPU 스키닝 활성 여부 */
	bool bGPUSkinningActive = false;

	// --- Voxel Data ---
	TSharedPtr<FHktVoxelRenderCache> EntityRenderCache;
	TUniquePtr<FHktVoxelMeshScheduler> MeshScheduler;
	FHktVoxelSkinAssembler SkinAssembler;

	// --- Default Voxel Skin Assets (에디터/블루프린트에서 설정) ---

	/** 레이어별 기본 복셀 스킨 에셋 (BoneGroups 포함 시 GPU 스키닝 활성화) */
	UPROPERTY(EditDefaultsOnly, Category = "HKT|VoxelSkin")
	TObjectPtr<UHktVoxelSkinLayerAsset> DefaultBodyAsset;

	UPROPERTY(EditDefaultsOnly, Category = "HKT|VoxelSkin")
	TObjectPtr<UHktVoxelSkinLayerAsset> DefaultHeadAsset;

	UPROPERTY(EditDefaultsOnly, Category = "HKT|VoxelSkin")
	TObjectPtr<UHktVoxelSkinLayerAsset> DefaultArmorAsset;

	/** 레이어 → 기본 에셋 매핑 헬퍼 */
	UHktVoxelSkinLayerAsset* GetDefaultAssetForLayer(EHktVoxelSkinLayer::Type Layer) const;

	/** GPU 스키닝: 본 이름 → 본 인덱스(1~) 매핑 */
	TMap<FName, uint8> BoneNameToIndex;

	// --- Cached State ---
	FHktEntityId CachedEntityId = InvalidEntityId;
	FVector InterpLocation = FVector::ZeroVector;
	FVector CachedRenderLocation = FVector::ZeroVector;
	FRotator InterpRotation = FRotator::ZeroRotator;
	FRotator CachedRotation = FRotator::ZeroRotator;

	uint16 CachedSkinSetID = 0;
	uint8  CachedPaletteRow = 0;

	/** 캐시된 AnimInstance */
	TWeakObjectPtr<UHktAnimInstance> CachedAnimInstance;

	/** 정적 모드 청크 좌표 — 원점(0,0,0) */
	static const FIntVector EntityChunkCoord;
};
