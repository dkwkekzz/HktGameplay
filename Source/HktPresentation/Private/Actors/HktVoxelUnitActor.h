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
class FHktVoxelRenderCache;
struct FHktEntityPresentation;
struct FHktVoxelBoneGroup;

/**
 * 복셀 캐릭터/유닛용 Actor.
 *
 * 기존 AHktUnitActor(SkeletalMesh 기반)의 복셀 버전.
 * UHktVoxelChunkComponent를 사용하여 Greedy Meshed 복셀 캐릭터를 렌더링한다.
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

	/** 본별 청크 초기화 (본-리지드 애니메이션 모드 진입) */
	void InitializeBoneChunks(const TArray<FHktVoxelBoneGroup>& BoneGroups);

	/** 본별 청크 해제 (정적 모드로 복귀) */
	void TeardownBoneChunks();

	/** HiddenSkeleton의 AnimInstance 캐시 반환 */
	UHktAnimInstance* GetAnimInstance();

	// --- Components ---
	UPROPERTY(VisibleAnywhere, Category = "HKT|Voxel")
	TObjectPtr<USceneComponent> RootScene;

	/** 단일 청크 (정적 모드 / 폴백) */
	UPROPERTY(VisibleAnywhere, Category = "HKT|Voxel")
	TObjectPtr<UHktVoxelChunkComponent> BodyChunk;

	/** 숨긴 스켈레톤 — 본 트랜스폼 구동용 (렌더링 안 함) */
	UPROPERTY(VisibleAnywhere, Category = "HKT|Voxel")
	TObjectPtr<USkeletalMeshComponent> HiddenSkeleton;

	/** 본별 청크 컴포넌트 (본-리지드 모드) */
	TMap<FName, TObjectPtr<UHktVoxelChunkComponent>> BoneChunks;

	/** 본별 청크 좌표 — 공유 RenderCache에서 본 구분용 */
	TMap<FName, FIntVector> BoneChunkCoords;

	/** 본-리지드 모드 활성 여부 */
	bool bBoneAnimatedMode = false;

	// --- Voxel Data ---
	TSharedPtr<FHktVoxelRenderCache> EntityRenderCache;
	TUniquePtr<FHktVoxelMeshScheduler> MeshScheduler;
	FHktVoxelSkinAssembler SkinAssembler;

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
