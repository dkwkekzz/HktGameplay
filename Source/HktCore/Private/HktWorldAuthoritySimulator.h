// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktSimulator.h"
#include "HktSimulationSystems.h"

class FHktVMRuntimePool;
class FHktVMInterpreter;
struct FHktVMHandle;
struct FHktPendingEvent;

// ============================================================================
// FHktWorldAuthoritySimulator (구 FHktSimulationWorld)
//
// 파이프라인: Arrange → Build → Process → Physics → Cleanup
// ============================================================================

class HKTCORE_API FHktWorldAuthoritySimulator : public IHktAuthoritySimulator
{
public:
    FHktWorldAuthoritySimulator();
    ~FHktWorldAuthoritySimulator();

    virtual FHktSimulationDiff AdvanceFrame(const FHktSimulationEvent& InEvent) override;
    virtual FHktPlayerState ExportPlayerState(int64 OwnerHash) const override;
    virtual void ImportPlayerState(const FHktPlayerState& InState) override;
    virtual void ImportEntityStates(const TArray<FHktEntityState>& InStates) override;
    virtual const FHktWorldState& GetWorldState() const override { return WorldState; }
    virtual void RestoreWorldState(const FHktWorldState& InState) override;

private:
    void ProcessBatch(const FHktSimulationEvent& Event);

    FHktSchemaRegistry SchemaRegistry;
    FHktWorldState WorldState;

    TUniquePtr<FHktVMRuntimePool> VMPool;
    TUniquePtr<FHktVMInterpreter> Interpreter;

    TArray<FHktVMHandle> ActiveVMs;
    TArray<FHktVMHandle> CompletedVMs;
    TArray<FHktPhysicsEvent> GeneratedPhysicsEvents;
    TArray<FHktPendingEvent> PendingExternalEvents;
    TArray<FHktEntityId> FrameRemovedEntities;

    FHktEntityArrangeSystem EntityArrangeSystem;
    FHktVMBuildSystem       VMBuildSystem;
    FHktVMProcessSystem     VMProcessSystem;
    FHktPhysicsSystem       PhysicsSystem;
    FHktVMCleanupSystem     VMCleanupSystem;
};
