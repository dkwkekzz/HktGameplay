// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldDeterminismSimulator.h"
#include "VM/HktVMRuntime.h"
#include "VM/HktVMInterpreter.h"
#include "VM/HktVMContext.h"
#include "VM/HktVMProgram.h"
#include "HktCoreProperties.h"
#include "HktSimulationLimits.h"

#if ENABLE_HKT_INSIGHTS
#include "HktCoreDataCollector.h"
#include "HktCoreDefs.h"
#endif

FHktWorldDeterminismSimulator::FHktWorldDeterminismSimulator(const FString& InSourceName)
    : SourceName(InSourceName)
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
                          *VMPool, ActiveVMs, WorldState, VMProxy, SourceName);

    VMProcessSystem.Process(ActiveVMs, CompletedVMs, *VMPool,
                            Event.DeltaSeconds, PendingExternalEvents);

    MovementSystem.Process(WorldState, VMProxy, GeneratedMoveEndEvents);
    for (const FHktPendingEvent& ME : GeneratedMoveEndEvents)
    {
        PendingExternalEvents.Add(ME);
    }

    PhysicsSystem.Process(WorldState, VMProxy, GeneratedPhysicsEvents);
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

    VMCleanupSystem.Process(CompletedVMs, *VMPool, WorldState, VMProxy);
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

    VMProxy.ForEachDirtyEntity(WorldState, [&](FHktEntityId Id, int32 Slot, uint64 Mask)
    {
        if (Id >= PrevNext) return;
        uint64 M = Mask;
        while (M)
        {
            uint16 PropId = static_cast<uint16>(FMath::CountTrailingZeros64(M));
            int32 NewVal = WorldState.Get(Slot, PropId);
            int32 OldVal = VMProxy.GetPreFrameValue(Slot, PropId);
            Diff.PropertyDeltas.Add({ Id, PropId, NewVal, OldVal });
            M &= M - 1;
        }
    });
    VMProxy.ForEachTagDirtyEntity(WorldState, [&](FHktEntityId Id, int32 Slot)
    {
        if (Id >= PrevNext) return;
        FHktTagDelta Delta;
        Delta.EntityId = Id;
        Delta.Tags = WorldState.GetTagsBySlot(Slot);
        Delta.OldTags = VMProxy.GetPreFrameTags(Slot);
        Diff.TagDeltas.Add(MoveTemp(Delta));
    });
    VMProxy.ForEachOwnerDirtyEntity(WorldState, [&](FHktEntityId Id, int32 Slot)
    {
        if (Id >= PrevNext) return;
        FHktOwnerDelta Delta;
        Delta.EntityId = Id;
        Delta.NewOwnerUid = WorldState.OwnerUids[Slot];
        Delta.OldOwnerUid = VMProxy.GetPreFrameOwnerUid(Slot);
        Diff.OwnerDeltas.Add(Delta);
    });

#if ENABLE_HKT_INSIGHTS
    if (!SourceName.IsEmpty())
    {
        // 카테고리: "WorldState.{SourceName}" (예: "WorldState.Server", "WorldState.Client")
        const FString WsCat = FString::Printf(TEXT("WorldState.%s"), *SourceName);
        HKT_INSIGHT_CLEAR_CATEGORY(WsCat);

        // 프레임 메타 정보
        HKT_INSIGHT_COLLECT(WsCat, TEXT("_Frame"),
            FString::Printf(TEXT("%lld"), WorldState.FrameNumber));
        HKT_INSIGHT_COLLECT(WsCat, TEXT("_EntityCount"),
            FString::FromInt(WorldState.GetEntityCount()));

        // 엔티티별 속성 요약
        WorldState.ForEachEntity([&](FHktEntityId Id, int32 Slot)
        {
            FString PropSummary;

#if !UE_BUILD_SHIPPING
            // 엔티티 디버그 이름 (비-Shipping 전용)
            const FHktWorldState::FHktEntityDebugInfo* DebugInfo = WorldState.GetEntityDebugInfo(Id);
            if (DebugInfo && !DebugInfo->DebugName.IsEmpty())
            {
                PropSummary += FString::Printf(TEXT("[%s] "), *DebugInfo->DebugName);
            }
#endif

            const FGameplayTagContainer& SlotTags = WorldState.GetTagsBySlot(Slot);
            PropSummary += FString::Printf(TEXT("Tags=%s"), *SlotTags.ToStringSimple());
            PropSummary += FString::Printf(TEXT(" | Owner=%lld"), WorldState.GetOwnerUid(Id));
            for (uint16 PropId = 0; PropId < PropertyId::MaxCount; ++PropId)
            {
                const TCHAR* PropName = GetPropertyName(PropId);
                if (!PropName) continue;
                int32 Val = WorldState.Get(Slot, PropId);
                if (Val == 0) continue;
                PropSummary += FString::Printf(TEXT(" | %s=%d"), PropName, Val);
            }

            FString EntityKey = FString::Printf(TEXT("E_%d"), Id);
            HKT_INSIGHT_COLLECT(WsCat, EntityKey, PropSummary);
        });
    }
#endif

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

TUniquePtr<IHktDeterminismSimulator> CreateDeterminismSimulator(const FString& InSourceName)
{
    return MakeUnique<FHktWorldDeterminismSimulator>(InSourceName);
}
