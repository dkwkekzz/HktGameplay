// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldDeterminismSimulator.h"
#include "VM/HktVMRuntime.h"
#include "VM/HktVMInterpreter.h"
#include "VM/HktVMContext.h"
#include "HktCoreProperties.h"
#include "HktSimulationLimits.h"

FHktWorldDeterminismSimulator::FHktWorldDeterminismSimulator()
{
    WorldState.Initialize();
    VMProxy.Initialize(WorldState);

    ActiveVMs.Reserve(HktLimits::MaxVMs);
    CompletedVMs.Reserve(HktLimits::MaxVMs);
    GeneratedPhysicsEvents.Reserve(HktLimits::MaxPhysicsEvents);
    PendingExternalEvents.Reserve(HktLimits::MaxPendingEvents);
    GeneratedMoveEndEvents.Reserve(HktLimits::MaxPendingEvents);
    FrameRemovedEntities.Reserve(256);
    EntityArrangeSystem.ScratchRemoveList.Reserve(HktLimits::MaxEntities);
    VMProcessSystem.ScratchEvents.Reserve(HktLimits::MaxPendingEvents);

    VMPool = MakeUnique<FHktVMRuntimePool>();
    Interpreter = MakeUnique<FHktVMInterpreter>();
    Interpreter->Initialize(&WorldState, &VMProxy);
    VMProcessSystem.Interpreter = Interpreter.Get();
}

FHktWorldDeterminismSimulator::~FHktWorldDeterminismSimulator() = default;

void FHktWorldDeterminismSimulator::ProcessBatch(const FHktSimulationEvent& Event)
{
    WorldState.FrameNumber = Event.FrameNumber;
    WorldState.RandomSeed = Event.RandomSeed;
    VMProxy.ResetDirtyIndices(WorldState);

    EntityArrangeSystem.Process(WorldState, Event.RemovedOwnerIds);
    FrameRemovedEntities = EntityArrangeSystem.ScratchRemoveList;

    VMBuildSystem.Process(Event.NewEvents, static_cast<int32>(Event.FrameNumber),
                          *VMPool, ActiveVMs, WorldState, VMProxy);

    VMProcessSystem.Process(ActiveVMs, CompletedVMs, *VMPool,
                            Event.DeltaSeconds, PendingExternalEvents);

    MovementSystem.Process(WorldState, VMProxy, Event.DeltaSeconds, GeneratedMoveEndEvents);
    for (const FHktPendingEvent& ME : GeneratedMoveEndEvents)
    {
        PendingExternalEvents.Add(ME);
    }

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

FHktSimulationDiff FHktWorldDeterminismSimulator::AdvanceFrame(const FHktSimulationEvent& InEvent)
{
    for (const FHktEntityState& ES : InEvent.NewEntityStates)
    {
        WorldState.ImportEntityState(ES);
    }

    FHktEntityId PrevNext = WorldState.NextEntityId;

    // 제거 대상 엔티티 상태를 ProcessBatch 전에 캡처 (UndoDiff 복원용)
    TArray<FHktEntityState> PreRemoveStates;
    for (int64 OwnerId : InEvent.RemovedOwnerIds)
    {
        WorldState.ForEachEntityByOwner(OwnerId, [&](FHktEntityId Id, int32)
        {
            PreRemoveStates.Add(WorldState.ExtractEntityState(Id));
        });
    }

    ProcessBatch(InEvent);

    FHktSimulationDiff Diff;
    Diff.FrameNumber = InEvent.FrameNumber;
    Diff.PrevNextEntityId = PrevNext;
    Diff.RemovedEntities = MoveTemp(FrameRemovedEntities);
    Diff.RemovedEntityStates = MoveTemp(PreRemoveStates);

    for (FHktEntityId Id = PrevNext; Id < WorldState.NextEntityId; ++Id)
        if (WorldState.IsValidEntity(Id))
            Diff.SpawnedEntities.Add(WorldState.ExtractEntityState(Id));

    for (int32 T = 1; T < HktType::MaxTypes; ++T)
    {
        const FHktEntityPool& Pool = WorldState.GetPool(static_cast<FHktTypeId>(T));
        const FHktVMEntityPoolProxy& Proxy = VMProxy.GetProxy(static_cast<FHktTypeId>(T));
        if (Pool.Stride == 0) continue;
        const FHktEntitySchema& Schema = FHktSchemaRegistry::Get().Get(static_cast<FHktTypeId>(T));
        Proxy.ForEachDirtyEntity(Pool, [&](FHktEntityId Id, int32 Slot, uint32 Mask)
        {
            if (Id >= PrevNext) return;
            const int32* ED = Pool.EntityData(Slot);
            uint32 M = Mask;
            while (M)
            {
                int32 LP = FMath::CountTrailingZeros(M);
                int32 OldVal = Proxy.GetPreFrameValue(Pool, Slot, LP);
                Diff.PropertyDeltas.Add({ Id, Schema.PropertyIds[LP], ED[LP], OldVal });
                M &= M - 1;
            }
        });
        Proxy.ForEachTagDirtyEntity(Pool, [&](FHktEntityId Id, int32 Slot)
        {
            if (Id >= PrevNext) return;
            FHktTagDelta Delta;
            Delta.EntityId = Id;
            Delta.Tags = Pool.GetTags(Slot);
            Delta.OldTags = Proxy.GetPreFrameTags(Slot);
            Diff.TagDeltas.Add(MoveTemp(Delta));
        });
        Proxy.ForEachOwnerDirtyEntity(Pool, [&](FHktEntityId Id, int32 Slot)
        {
            if (Id >= PrevNext) return;
            FHktOwnerDelta Delta;
            Delta.EntityId = Id;
            Delta.NewOwnerUid = Pool.OwnerUids[Slot];
            Delta.OldOwnerUid = Proxy.GetPreFrameOwnerUid(Slot);
            Diff.OwnerDeltas.Add(Delta);
        });
    }
    return Diff;
}

FHktPlayerState FHktWorldDeterminismSimulator::ExportPlayerState(int64 OwnerUid) const
{
    FHktPlayerState Out;
    Out.PlayerUid = OwnerUid;

    WorldState.ForEachEntityByOwner(OwnerUid, [&](FHktEntityId Id, int32 /*Slot*/)
    {
        Out.OwnedEntities.Add(WorldState.ExtractEntityState(Id));
    });

    for (const FHktEvent& E : WorldState.ActiveEvents)
        if (WorldState.IsValidEntity(E.SourceEntity))
            if (WorldState.GetOwnerUid(E.SourceEntity) == OwnerUid)
                Out.ActiveEvents.Add(E);

    return Out;
}

void FHktWorldDeterminismSimulator::RestoreWorldState(const FHktWorldState& InState)
{
    WorldState.CopyFrom(InState);
}

void FHktWorldDeterminismSimulator::UndoDiff(const FHktSimulationDiff& Diff)
{
    WorldState.UndoDiff(Diff);
}

// ============================================================================
// Factory
// ============================================================================

TUniquePtr<IHktDeterminismSimulator> CreateDeterminismSimulator()
{
    return MakeUnique<FHktWorldDeterminismSimulator>();
}
