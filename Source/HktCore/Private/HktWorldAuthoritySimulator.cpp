// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldAuthoritySimulator.h"
#include "VM/HktVMRuntime.h"
#include "VM/HktVMInterpreter.h"
#include "VM/HktVMContext.h"
#include "HktPropertyIds.h"
#include "HktSimulationLimits.h"

FHktWorldAuthoritySimulator::FHktWorldAuthoritySimulator()
{
    SchemaRegistry.Initialize();
    WorldState.Initialize(SchemaRegistry);

    ActiveVMs.Reserve(HktLimits::MaxVMs);
    CompletedVMs.Reserve(HktLimits::MaxVMs);
    GeneratedPhysicsEvents.Reserve(HktLimits::MaxPhysicsEvents);
    PendingExternalEvents.Reserve(HktLimits::MaxPendingEvents);
    FrameRemovedEntities.Reserve(256);
    EntityArrangeSystem.ScratchRemoveList.Reserve(HktLimits::MaxEntities);
    VMProcessSystem.ScratchEvents.Reserve(HktLimits::MaxPendingEvents);

    VMPool = MakeUnique<FHktVMRuntimePool>();
    Interpreter = MakeUnique<FHktVMInterpreter>();
    Interpreter->Initialize(&WorldState);
    VMProcessSystem.Interpreter = Interpreter.Get();
}

FHktWorldAuthoritySimulator::~FHktWorldAuthoritySimulator() = default;

void FHktWorldAuthoritySimulator::ProcessBatch(const FHktSimulationEvent& Event)
{
    WorldState.FrameNumber = Event.FrameNumber;
    WorldState.RandomSeed = Event.RandomSeed;
    WorldState.ResetDirtyIndices();

    EntityArrangeSystem.Process(WorldState, Event.RemovedOwnerIds);
    FrameRemovedEntities = EntityArrangeSystem.ScratchRemoveList;

    VMBuildSystem.Process(Event.Events, static_cast<int32>(Event.FrameNumber),
                          *VMPool, ActiveVMs, WorldState);

    VMProcessSystem.Process(ActiveVMs, CompletedVMs, *VMPool,
                            Event.DeltaSeconds, PendingExternalEvents);

    PhysicsSystem.Process(WorldState, GeneratedPhysicsEvents);
    for (const FHktPhysicsEvent& PE : GeneratedPhysicsEvents)
    {
        FHktPendingEvent PA;
        PA.Type = EWaitEventType::Collision;
        PA.WatchedEntity = PE.EntityA;
        PA.HitEntity = PE.EntityB;
        PendingExternalEvents.Add(PA);

        FHktPendingEvent PB;
        PB.Type = EWaitEventType::Collision;
        PB.WatchedEntity = PE.EntityB;
        PB.HitEntity = PE.EntityA;
        PendingExternalEvents.Add(PB);
    }

    VMCleanupSystem.Process(CompletedVMs, *VMPool, WorldState);
}

FHktSimulationDiff FHktWorldAuthoritySimulator::AdvanceFrame(const FHktSimulationEvent& InEvent)
{
    FHktEntityId PrevNext = WorldState.NextEntityId;
    ProcessBatch(InEvent);

    FHktSimulationDiff Diff;
    Diff.FrameNumber = InEvent.FrameNumber;
    Diff.RemovedEntities = MoveTemp(FrameRemovedEntities);

    for (FHktEntityId Id = PrevNext; Id < WorldState.NextEntityId; ++Id)
        if (WorldState.IsValidEntity(Id))
            Diff.SpawnedEntities.Add(WorldState.ExtractEntityState(Id));

    for (int32 T = 1; T < HktType::MaxTypes; ++T)
    {
        const FHktEntityPool& Pool = WorldState.GetPool(static_cast<FHktTypeId>(T));
        if (!Pool.Schema) continue;
        Pool.ForEachDirtyEntity([&](FHktEntityId Id, int32 Slot, uint32 Mask)
        {
            if (Id >= PrevNext) return;
            const int32* ED = Pool.EntityData(Slot);
            uint32 M = Mask;
            while (M)
            {
                int32 LP = FMath::CountTrailingZeros(M);
                Diff.PropertyDeltas.Add({ Id, Pool.Schema->PropertyIds[LP], ED[LP] });
                M &= M - 1;
            }
        });
    }
    return Diff;
}

FHktPlayerState FHktWorldAuthoritySimulator::ExportPlayerState(int64 OwnerHash) const
{
    FHktPlayerState Out;
    Out.PlayerUid = OwnerHash;

    for (int32 T = 1; T < HktType::MaxTypes; ++T)
    {
        const FHktEntitySchema& Sch = SchemaRegistry.Get(static_cast<FHktTypeId>(T));
        int8 OwnerLP = Sch.GetLocalIndex(PropertyId::OwnerPlayerHash);
        if (OwnerLP == -1) continue;

        const FHktEntityPool& Pool = WorldState.GetPool(static_cast<FHktTypeId>(T));
        Pool.ForEachEntity([&](FHktEntityId Id, int32 Slot)
        {
            if (static_cast<int64>(Pool.EntityData(Slot)[OwnerLP]) == OwnerHash)
                Out.OwnedEntities.Add(WorldState.ExtractEntityState(Id));
        });
    }

    for (const FHktEvent& E : WorldState.ActiveEvents)
        if (WorldState.IsValidEntity(E.SourceEntity))
            if (static_cast<int64>(WorldState.GetProperty(E.SourceEntity, PropertyId::OwnerPlayerHash)) == OwnerHash)
                Out.ActiveEvents.Add(E);

    return Out;
}

void FHktWorldAuthoritySimulator::ImportPlayerState(const FHktPlayerState& InState)
{
    for (const FHktEntityState& ES : InState.OwnedEntities)
        WorldState.ImportEntityState(ES);
    WorldState.ActiveEvents.Append(InState.ActiveEvents);
}

void FHktWorldAuthoritySimulator::ImportEntityStates(const TArray<FHktEntityState>& InStates)
{
    for (const FHktEntityState& ES : InStates)
    {
        WorldState.ImportEntityState(ES);
    }
}

void FHktWorldAuthoritySimulator::RestoreWorldState(const FHktWorldState& InState)
{
    WorldState.CopyFrom(InState);
}

// ============================================================================
// Factory
// ============================================================================

TUniquePtr<IHktAuthoritySimulator> CreateAuthoritySimulator()
{
    return MakeUnique<FHktWorldAuthoritySimulator>();
}
