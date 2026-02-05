// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/HktWorldState.h"
#include "Physics/HktSpatialSystem.h"
#include "HktCoreInterfaces.h"

/**
 * FHktSimulationWorld - 시뮬레이션 메인 루프 코디네이터
 *
 * HktCore의 모든 서브시스템(WorldState, SpatialSystem, VMProcessor)을 소유하고
 * 결정론적 3-Phase 틱 루프를 실행합니다.
 *
 * Tick은 FrameNumber를 인자로 받으며, DeltaTime 대신
 * 내부적으로 고정 타임스텝을 사용하여 결정론을 보장합니다.
 *
 * [Phase 1] Input Processing (Intent)
 *   VMProcessor가 외부에서 수신된 FHktIntentEvent 큐를 모두 비움.
 *   이동, 스킬 사용 등의 1차적인 상태 변화가 WorldState에 반영.
 *
 * [Phase 2] Spatial Update & Collision
 *   변경된 위치 정보를 SpatialSystem에 업데이트.
 *   충돌이 감지되면 FHktCollisionPair 리스트를 생성.
 *
 * [Phase 3] Reaction Processing (System)
 *   충돌 결과를 VMProcessor에 전달하여 즉시 처리.
 *   1프레임 지연 없이 충돌 반응(체력 감소, 넉백 등)이 확정.
 *
 * UObject/UWorld 참조 없음 — 순수 C++ 시뮬레이션.
 */
class HKTCORE_API FHktSimulationWorld
{
public:
    FHktSimulationWorld();
    ~FHktSimulationWorld();

    /** 서브시스템 초기화 (생성 후 반드시 호출) */
    void Initialize();

    /** 종료 및 정리 */
    void Shutdown();

    // ========== Phased Execution Loop ==========

    /**
     * 결정론적 3-Phase 틱 실행
     * @param FrameNumber - 프레임 번호 (결정론 키)
     * @param DeltaSeconds - 프레임 간 시간 (VM 타이머용)
     */
    void Tick(uint32 FrameNumber, float DeltaSeconds);

    // ========== 외부 입력 ==========

    /** Intent 이벤트 추가 (PlayerController → Server → 여기로) */
    void AddInputEvent(const FHktIntentEvent& Event);

    // ========== 상태 접근 ==========

    const FHktWorldState& GetWorldState() const { return WorldState; }
    FHktWorldState& GetWorldStateMutable() { return WorldState; }

    const FHktSpatialSystem& GetSpatialSystem() const { return SpatialSystem; }
    FHktSpatialSystem& GetSpatialSystemMutable() { return SpatialSystem; }

    /** VMProcessor 접근 (IHktVMProcessorInterface 반환) */
    IHktVMProcessorInterface* GetVMProcessor() const { return VMProcessor.Get(); }

    /** 어댑터 접근 (기존 IHktMasterStashInterface 호환용) */
    IHktMasterStashInterface* GetWorldStateAdapter() const { return WorldStateAdapter.Get(); }

private:
    FHktWorldState WorldState;
    FHktSpatialSystem SpatialSystem;

    /** VMProcessor — 기존 IHktVMProcessorInterface를 통해 접근 */
    TUniquePtr<IHktVMProcessorInterface> VMProcessor;

    /** WorldState + SpatialSystem을 IHktMasterStashInterface로 래핑 */
    TUniquePtr<FHktWorldStateAdapter> WorldStateAdapter;

    /** 충돌 이벤트 태그 (ON_COLLISION) */
    FGameplayTag CollisionEventTag;
};
