// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/HktTypes.h"
#include "Common/HktEvents.h"
#include "Physics/HktCollisionShapes.h"

// Forward declarations
class FHktWorldState;

/**
 * FHktSpatialSystem - Cell 기반 공간 관리 및 충돌 감지
 *
 * PhysicsWorld + MasterStash 셀 관리를 통합.
 * - Cell 기반 공간 분할: 충돌 감지(Detection) 및 네트워크 관심 영역(Relevance) 관리
 * - 충돌 감지: Watched 엔티티 기반 + 전체 충돌체 간 감지
 * - 쿼리 API: OverlapSphere, Raycast, SweepSphere
 *
 * 충돌 발생 시 직접 데이터를 수정하지 않고, 결과를 반환하여 VM에 위임.
 */
class HKTCORE_API FHktSpatialSystem
{
public:
    FHktSpatialSystem();
    ~FHktSpatialSystem();

    // Non-copyable
    FHktSpatialSystem(const FHktSpatialSystem&) = delete;
    FHktSpatialSystem& operator=(const FHktSpatialSystem&) = delete;

    // ========================================================================
    // 초기화 / 종료
    // ========================================================================

    void Initialize(FHktWorldState* InWorldState);
    void Shutdown();

    // ========================================================================
    // Cell 기반 공간 관리 (MasterStash에서 이동)
    // ========================================================================

    /** 셀 크기 설정 (cm 단위) */
    void SetCellSize(float InCellSize);
    float GetCellSize() const { return CellSize; }

    /** 엔티티의 현재 셀 조회 */
    FIntPoint GetEntityCell(FHktEntityId Entity) const;

    /** 특정 셀 내의 모든 엔티티 조회 */
    const TSet<FHktEntityId>* GetEntitiesInCell(FIntPoint Cell) const;

    /** 이번 프레임의 셀 변경 이벤트 가져오기 (호출 후 클리어됨) */
    TArray<FHktCellChangeEvent> ConsumeCellChangeEvents();

    /** 여러 셀의 모든 엔티티 수집 */
    void GetEntitiesInCells(const TSet<FIntPoint>& Cells, TSet<FHktEntityId>& OutEntities) const;

    /**
     * 위치 변경된 엔티티들의 셀 갱신
     * SimulationWorld::Tick Phase 2에서 호출
     */
    void UpdateEntityPositions();

    /** 엔티티 생성 시 셀 초기화 */
    void OnEntityAllocated(FHktEntityId Entity);

    /** 엔티티 삭제 시 셀 정리 */
    void OnEntityFreed(FHktEntityId Entity);

    // ========================================================================
    // Watch 기반 충돌 감지
    // ========================================================================

    /**
     * Watched 엔티티의 충돌 감지
     * @return 감지된 충돌 수
     */
    int32 DetectWatchedCollisions(TArray<FHktCollisionPair>& OutCollisions);

    /** 전체 충돌체 간 광역 충돌 감지 */
    int32 DetectAllCollisions(TArray<FHktCollisionPair>& OutPairs);

    /** WaitCollision 시작 시 호출 */
    void AddWatchedEntity(FHktEntityId Entity);

    /** WaitCollision 종료 시 호출 */
    void RemoveWatchedEntity(FHktEntityId Entity);

    /** 엔티티가 Watch 중인지 확인 */
    bool IsWatched(FHktEntityId Entity) const;

    /** 모든 Watch 클리어 */
    void ClearWatchedEntities();

    // ========================================================================
    // 쿼리 API
    // ========================================================================

    int32 OverlapSphere(
        const FVector& Center,
        float Radius,
        TArray<FHktEntityId>& OutEntities,
        uint8 LayerMask = HktPhysics::Layer::All,
        FHktEntityId ExcludeEntity = InvalidEntityId) const;

    int32 OverlapSphereReset(
        const FVector& Center,
        float Radius,
        TArray<FHktEntityId>& OutEntities,
        uint8 LayerMask = HktPhysics::Layer::All,
        FHktEntityId ExcludeEntity = InvalidEntityId) const;

    bool Raycast(
        const FVector& Origin,
        const FVector& Direction,
        float MaxDistance,
        FHktRaycastResult& OutResult,
        uint8 LayerMask = HktPhysics::Layer::All,
        FHktEntityId ExcludeEntity = InvalidEntityId) const;

    bool SweepSphere(
        const FVector& Start,
        const FVector& End,
        float Radius,
        FHktSweepResult& OutResult,
        uint8 LayerMask = HktPhysics::Layer::All,
        FHktEntityId ExcludeEntity = InvalidEntityId) const;

    bool TestEntityOverlap(FHktEntityId EntityA, FHktEntityId EntityB) const;

    bool TestEntityCollision(
        FHktEntityId EntityA,
        FHktEntityId EntityB,
        FHktCollisionResult& OutResult) const;

    // ========================================================================
    // 충돌체 정보 조회
    // ========================================================================

    EHktColliderType GetColliderType(FHktEntityId Entity) const;
    uint8 GetCollisionLayer(FHktEntityId Entity) const;
    uint8 GetCollisionMask(FHktEntityId Entity) const;
    bool IsValidCollider(FHktEntityId Entity) const;
    bool CanCollide(FHktEntityId EntityA, FHktEntityId EntityB) const;

    /** 활성 충돌체 목록 갱신 플래그 설정 */
    void MarkActiveCollidersDirty() { bActiveCollidersDirty = true; }

    // ========================================================================
    // 디버그
    // ========================================================================

#if !UE_BUILD_SHIPPING
    int32 GetActiveColliderCount() const;
    int32 GetWatchedEntityCount() const { return WatchedEntities.Num(); }
    FString GetDebugString() const;
    FString GetColliderDebugString(FHktEntityId Entity) const;
#endif

private:
    // ========================================================================
    // 내부 헬퍼
    // ========================================================================

    FVector GetEntityPosition(FHktEntityId Entity) const;
    float GetColliderRadius(FHktEntityId Entity) const;
    float GetCapsuleHalfHeight(FHktEntityId Entity) const;
    void GetCapsuleEndpoints(FHktEntityId Entity, FVector& OutTop, FVector& OutBottom) const;
    bool PassesLayerFilter(FHktEntityId Entity, uint8 LayerMask) const;
    void RefreshActiveColliders() const;

    /** 위치 → 셀 변환 */
    FIntPoint PositionToCell(const FVector& Position) const;

    /** 엔티티의 셀 변경 처리 (내부용) */
    void UpdateEntityCell(FHktEntityId Entity, FIntPoint NewCell);

    // ========================================================================
    // 데이터
    // ========================================================================

    FHktWorldState* WorldState = nullptr;

    // Watch & Collision
    TSet<FHktEntityId> WatchedEntities;
    mutable TArray<FHktEntityId> ActiveColliders;
    mutable bool bActiveCollidersDirty = true;

    // Cell Spatial Index
    float CellSize = 5000.0f;  // 50m default
    TMap<FIntPoint, TSet<FHktEntityId>> CellToEntities;
    TArray<FIntPoint> EntityCells;
    TArray<FHktCellChangeEvent> PendingCellChangeEvents;
};
