// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktPhysicsTypes.h"
#include "HktTickable.h"

// Forward declarations
class IHktStashInterface;

/** 충돌 알림 콜백 타입 */
DECLARE_DELEGATE_TwoParams(FOnCollisionDetected, FHktEntityId /*WatchedEntity*/, FHktEntityId /*HitEntity*/);

/**
 * FHktPhysicsWorld - SOA 레이아웃 물리 월드
 * 
 * 설계 원칙:
 * - IHktTickable 구현: VMProcessor의 Tickables에 등록하여 매 프레임 실행
 * - Stash Property 직접 읽기: 데이터 복사 없음
 * - 콜백 기반 알림: VMProcessor 직접 참조 대신 델리게이트 사용
 * 
 * 사용 흐름:
 * 1. CreatePhysicsWorld()로 생성
 * 2. SetOnCollisionDetected()로 콜백 등록
 * 3. VMProcessor에 RegisterTickable() 또는 수동 Tick() 호출
 * 4. VM에서 WaitCollision 시 AddWatchedEntity() 직접 호출
 * 5. Tick()에서 충돌 감지 → 콜백 호출
 */
class HKTCORE_API FHktPhysicsWorld : public IHktTickable
{
public:
    FHktPhysicsWorld();
    virtual ~FHktPhysicsWorld();
    
    // Non-copyable
    FHktPhysicsWorld(const FHktPhysicsWorld&) = delete;
    FHktPhysicsWorld& operator=(const FHktPhysicsWorld&) = delete;
    
    // ========================================================================
    // 초기화 / 종료
    // ========================================================================
    
    /**
     * 물리 월드 초기화
     * @param InStash - 엔티티 데이터 소스
     */
    void Initialize(IHktStashInterface* InStash);
    
    /** 종료 및 정리 */
    void Shutdown();
    
    // ========================================================================
    // IHktTickable 구현
    // ========================================================================
    
    /** 매 프레임 충돌 감지 수행 */
    virtual void Tick(float DeltaSeconds) override;
    
    /** 틱 우선순위: Physics (100) */
    virtual int32 GetTickPriority() const override { return HktTickPriority::Physics; }
    
    /** 틱 활성화 여부 */
    virtual bool IsTickEnabled() const override { return bTickEnabled && Stash != nullptr; }
    
    /** 디버그 이름 */
    virtual FName GetTickableName() const override { return FName(TEXT("PhysicsWorld")); }
    
    /** 틱 활성화/비활성화 */
    void SetTickEnabled(bool bEnabled) { bTickEnabled = bEnabled; }
    
    // ========================================================================
    // 콜백 설정
    // ========================================================================
    
    /**
     * 충돌 감지 콜백 설정
     * WaitCollision 중인 엔티티가 충돌하면 호출됨
     * 
     * @param Callback - (WatchedEntity, HitEntity) 콜백
     * 
     * 사용 예 (VMInterpreter에서):
     *   PhysicsWorld->SetOnCollisionDetected(
     *       FOnCollisionDetected::CreateRaw(this, &FHktVMInterpreter::OnPhysicsCollision));
     */
    void SetOnCollisionDetected(FOnCollisionDetected Callback) { OnCollisionDetected = Callback; }
    
    // ========================================================================
    // Watch 관리 (VM에서 직접 호출)
    // ========================================================================
    
    /** WaitCollision 시작 시 호출 */
    void AddWatchedEntity(FHktEntityId Entity);
    
    /** WaitCollision 종료 시 호출 */
    void RemoveWatchedEntity(FHktEntityId Entity);
    
    /** 엔티티가 Watch 중인지 확인 */
    bool IsWatched(FHktEntityId Entity) const;
    
    /** 모든 Watch 클리어 */
    void ClearWatchedEntities();
    
    // ========================================================================
    // 활성 충돌체 관리
    // ========================================================================
    
    /**
     * 활성 충돌체 목록 갱신 플래그 설정
     * Stash에서 엔티티 생성/삭제 시 호출
     */
    void MarkActiveCollidersDirty() { bActiveCollidersDirty = true; }
    
    // ========================================================================
    // 쿼리 API (언제든 호출 가능)
    // ========================================================================
    
    /**
     * 구 범위 내 엔티티 검색 (Overlap)
     * @param Center - 중심 위치
     * @param Radius - 검색 반경 (cm)
     * @param OutEntities - 결과 배열 (기존 내용 유지, 추가됨)
     * @param LayerMask - 대상 레이어 마스크 (기본: 모두)
     * @param ExcludeEntity - 제외할 엔티티 (보통 Self)
     * @return 찾은 엔티티 수
     */
    int32 OverlapSphere(
        const FVector& Center, 
        float Radius,
        TArray<FHktEntityId>& OutEntities,
        uint8 LayerMask = HktPhysics::Layer::All,
        FHktEntityId ExcludeEntity = InvalidEntityId) const;
    
    /**
     * 구 범위 내 엔티티 검색 (결과 배열 초기화 포함)
     */
    int32 OverlapSphereReset(
        const FVector& Center,
        float Radius,
        TArray<FHktEntityId>& OutEntities,
        uint8 LayerMask = HktPhysics::Layer::All,
        FHktEntityId ExcludeEntity = InvalidEntityId) const;
    
    /**
     * 레이캐스트
     */
    bool Raycast(
        const FVector& Origin,
        const FVector& Direction,
        float MaxDistance,
        FHktRaycastResult& OutResult,
        uint8 LayerMask = HktPhysics::Layer::All,
        FHktEntityId ExcludeEntity = InvalidEntityId) const;
    
    /**
     * 구 스윕 테스트 (이동 경로 상 충돌 검사)
     */
    bool SweepSphere(
        const FVector& Start,
        const FVector& End,
        float Radius,
        FHktSweepResult& OutResult,
        uint8 LayerMask = HktPhysics::Layer::All,
        FHktEntityId ExcludeEntity = InvalidEntityId) const;
    
    /** 두 엔티티 간 Overlap 테스트 */
    bool TestEntityOverlap(FHktEntityId EntityA, FHktEntityId EntityB) const;
    
    /** 두 엔티티 간 상세 충돌 테스트 */
    bool TestEntityCollision(
        FHktEntityId EntityA, 
        FHktEntityId EntityB,
        FHktCollisionResult& OutResult) const;
    
    /** 모든 활성 충돌체 간 광역 충돌 감지 */
    int32 DetectAllCollisions(TArray<FHktCollisionPair>& OutPairs);
    
    // ========================================================================
    // 충돌체 정보 조회 (Stash Property 래핑)
    // ========================================================================
    
    EHktColliderType GetColliderType(FHktEntityId Entity) const;
    uint8 GetCollisionLayer(FHktEntityId Entity) const;
    uint8 GetCollisionMask(FHktEntityId Entity) const;
    bool IsValidCollider(FHktEntityId Entity) const;
    bool CanCollide(FHktEntityId EntityA, FHktEntityId EntityB) const;
    
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
    
    /** 충돌 감지 수행 (Tick에서 호출) */
    void DetectCollisions();
    
    FVector GetEntityPosition(FHktEntityId Entity) const;
    float GetColliderRadius(FHktEntityId Entity) const;
    float GetCapsuleHalfHeight(FHktEntityId Entity) const;
    void GetCapsuleEndpoints(FHktEntityId Entity, FVector& OutTop, FVector& OutBottom) const;
    bool PassesLayerFilter(FHktEntityId Entity, uint8 LayerMask) const;
    void RefreshActiveColliders() const;
    
    // ========================================================================
    // 데이터
    // ========================================================================
    
    IHktStashInterface* Stash = nullptr;
    FOnCollisionDetected OnCollisionDetected;
    bool bTickEnabled = true;
    
    TSet<FHktEntityId> WatchedEntities;
    mutable TArray<FHktEntityId> ActiveColliders;
    mutable bool bActiveCollidersDirty = true;
};
