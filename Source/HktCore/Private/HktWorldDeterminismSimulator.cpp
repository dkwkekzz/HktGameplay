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

namespace HktInsightsInternal
{
    inline FString PropIdToName(uint16 PropId)
    {
        switch (PropId)
        {
        case PropertyId::PosX:            return TEXT("PosX");
        case PropertyId::PosY:            return TEXT("PosY");
        case PropertyId::PosZ:            return TEXT("PosZ");
        case PropertyId::RotYaw:          return TEXT("RotYaw");
        case PropertyId::MoveTargetX:     return TEXT("MoveTargX");
        case PropertyId::MoveTargetY:     return TEXT("MoveTargY");
        case PropertyId::MoveTargetZ:     return TEXT("MoveTargZ");
        case PropertyId::MoveForce:       return TEXT("MoveForce");
        case PropertyId::IsMoving:        return TEXT("IsMoving");
        case PropertyId::Health:          return TEXT("Health");
        case PropertyId::MaxHealth:       return TEXT("MaxHealth");
        case PropertyId::AttackPower:     return TEXT("AtkPow");
        case PropertyId::Defense:         return TEXT("Defense");
        case PropertyId::Team:            return TEXT("Team");
        case PropertyId::Mana:            return TEXT("Mana");
        case PropertyId::MaxMana:         return TEXT("MaxMana");
        case PropertyId::OwnerEntity:     return TEXT("OwnerEnt");
        case PropertyId::EntityType:      return TEXT("EntType");
        case PropertyId::TargetPosX:      return TEXT("TargPosX");
        case PropertyId::TargetPosY:      return TEXT("TargPosY");
        case PropertyId::TargetPosZ:      return TEXT("TargPosZ");
        case PropertyId::Param0:          return TEXT("Param0");
        case PropertyId::Param1:          return TEXT("Param1");
        case PropertyId::Param2:          return TEXT("Param2");
        case PropertyId::Param3:          return TEXT("Param3");
        case PropertyId::AnimState:       return TEXT("AnimState");
        case PropertyId::VisualState:     return TEXT("VisState");
        default:                          return FString::Printf(TEXT("P%d"), PropId);
        }
    }

    inline FString TypeIdToName(FHktTypeId TypeId)
    {
        switch (TypeId)
        {
        case HktType::Unit:       return TEXT("Unit");
        case HktType::Projectile: return TEXT("Projectile");
        case HktType::Equipment:  return TEXT("Equipment");
        case HktType::Building:   return TEXT("Building");
        default:                  return FString::Printf(TEXT("Type%d"), TypeId);
        }
    }
}
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
        for (int32 T = 1; T < HktType::MaxTypes; ++T)
        {
            const FHktEntityPool& Pool = WorldState.GetPool(static_cast<FHktTypeId>(T));
            if (Pool.ActiveCount == 0) continue;
            const FString TypeName = HktInsightsInternal::TypeIdToName(static_cast<FHktTypeId>(T));
            const FHktEntitySchema& Schema = FHktSchemaRegistry::Get().Get(static_cast<FHktTypeId>(T));

            Pool.ForEachEntity([&](FHktEntityId Id, int32 Slot)
            {
                // 키: "E_{EntityId}" — 값: "Type=Unit | Owner=123 | PosX=100 PosY=200 ..."
                FString PropSummary;
                PropSummary += FString::Printf(TEXT("Type=%s"), *TypeName);
                PropSummary += FString::Printf(TEXT(" | Owner=%lld"), WorldState.GetOwnerUid(Id));
                for (int8 LocalIdx = 0; LocalIdx < Pool.Stride; ++LocalIdx)
                {
                    PropSummary += FString::Printf(TEXT(" | %s=%d"),
                        *HktInsightsInternal::PropIdToName(Schema.PropertyIds[LocalIdx]),
                        Pool.Get(Slot, LocalIdx));
                }

                FString EntityKey = FString::Printf(TEXT("E_%d"), Id);
                HKT_INSIGHT_COLLECT(WsCat, EntityKey, PropSummary);
            });
        }
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
