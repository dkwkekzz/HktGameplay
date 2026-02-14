// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktVMTypes.h"
#include "HktSimulationSystems.h"
#include "HktSimulator.h"

// Forward declaration (Private)
class FHktVMInterpreter;
class FHktVMRuntimePool;
struct FHktSimWorldInternalData;

/**
 * FHktSimulationWorld
 * - 시뮬레이션의 진입점(Entry Point)이자 파사드(Facade)
 * - ProcessBatch() 내에서 결정론적 순서 보장 필수
 */
class HKTCORE_API FHktSimulationWorld : public IHktSimulator
{
public:
    FHktSimulationWorld();
    ~FHktSimulationWorld();

    /** 메인 틱 함수: Arrange -> Build -> Process -> Physics -> Commit -> Cleanup */
    virtual void ProcessBatch(const FHktSimulationEvent& Event) override;

    /** 롤백 지원: 특정 상태로 복구 */
    virtual void RestoreWorldState(const FHktWorldState& InState) override;

    /** 스냅샷 추출 */
    virtual void SnapshotWorldState(FHktWorldState& OutState) const override;

    /**
     * [Zero-Copy Access]
     * 렌더링 시스템이 사용할 View 객체를 생성(초기화).
     * 매 프레임 호출되어 가장 최신의 Local Overlay를 구성.
     */
    virtual void CreateWorldView(FHktWorldView& OutView) override;

private:
    // --- Data ---
    FHktWorldState WorldState;
    TUniquePtr<FHktVMRuntimePool> VMPool;

    TArray<FHktVMHandle> ActiveVMs;
    TArray<FHktVMHandle> CompletedVMs;
    TArray<FHktPhysicsEvent> GeneratedPhysicsEvents;
    TArray<FHktPendingEvent> PendingExternalEvents;

    // --- Internal Data (PIMPL: StorePool 등 Private 타입 포함) ---
    TUniquePtr<FHktSimWorldInternalData> InternalData;

    // --- Systems ---
    FHktEntityArrangeSystem  EntityArrangeSystem;
    FHktVMBuildSystem        VMBuildSystem;
    FHktVMProcessSystem      VMProcessSystem;
    FHktPhysicsSystem        PhysicsSystem;
    FHktApplyStoreSystem     ApplyStoreSystem;
    FHktVMCleanupSystem      VMCleanupSystem;
    FHktPublishViewSystem    PublishViewSystem;

    // --- Interpreter ---
    TUniquePtr<FHktVMInterpreter> Interpreter;
};
