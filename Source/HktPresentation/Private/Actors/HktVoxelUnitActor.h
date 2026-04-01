// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HktSelectable.h"
#include "IHktPresentableActor.h"
#include "HktVoxelSkinTypes.h"
#include "HktVoxelSkinAssembler.h"
#include "HktVoxelUnitActor.generated.h"

class UHktVoxelChunkComponent;
class FHktVoxelRenderCache;
class FHktVoxelMeshScheduler;
struct FHktEntityPresentation;

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

	// --- Components ---
	UPROPERTY(VisibleAnywhere, Category = "HKT|Voxel")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, Category = "HKT|Voxel")
	TObjectPtr<UHktVoxelChunkComponent> BodyChunk;

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

	/** 청크 좌표 — 엔티티 복셀은 항상 원점(0,0,0)에 1청크 */
	static const FIntVector EntityChunkCoord;
};
