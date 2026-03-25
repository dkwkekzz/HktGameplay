// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreSimulator.h"
#include "HktSimulationSystems.h"
#include "VM/HktVMWorldStateProxy.h"

class FHktVMRuntimePool;
class FHktVMInterpreter;
struct FHktVMHandle;
struct FHktPendingEvent;

// ============================================================================
// FHktWorldDeterminismSimulator
//
// 파이프라인: Arrange → Build → Process → Movement → Physics → Cleanup
// ============================================================================

class HKTCORE_API FHktWorldDeterminismSimulator : public IHktDeterminismSimulator
{
public:
    FHktWorldDeterminismSimulator(const FString& InSourceName);
    ~FHktWorldDeterminismSimulator();

    virtual FHktSimulationDiff AdvanceFrame(const FHktSimulationEvent& InEvent) override;
    virtual FHktPlayerState ExportPlayerState(int64 OwnerUid) const override;
    virtual const FHktWorldState& GetWorldState() const override { return WorldState; }
    virtual void RestoreWorldState(const FHktWorldState& InState) override;
    virtual void UndoDiff(const FHktSimulationDiff& Diff) override;

private:
    void ProcessBatch(const FHktSimulationEvent& Event);

    FString SourceName;
    FHktWorldState WorldState;
    FHktVMWorldStateProxy VMProxy;

    TUniquePtr<FHktVMRuntimePool> VMPool;
    TUniquePtr<FHktVMInterpreter> Interpreter;

    TArray<FHktVMHandle> ActiveVMs;
    TArray<FHktVMHandle> CompletedVMs;
    TArray<FHktPhysicsEvent> GeneratedPhysicsEvents;
    TArray<FHktPendingEvent> PendingExternalEvents;
    TArray<FHktPendingEvent> GeneratedMoveEndEvents;
    TArray<FHktEntityId> FrameRemovedEntities;
    TArray<FHktEvent> DispatchedEvents;

    FHktEntityArrangeSystem EntityArrangeSystem;
    FHktVMBuildSystem       VMBuildSystem;
    FHktVMProcessSystem     VMProcessSystem;
    FHktMovementSystem      MovementSystem;
    FHktPhysicsSystem       PhysicsSystem;
    FHktVMCleanupSystem     VMCleanupSystem;
};
