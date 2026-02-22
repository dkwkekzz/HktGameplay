// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationSystems.h"
#include "HktPropertyIds.h"
#include "VM/HktVMProgram.h"
#include "VM/HktVMRuntime.h"
#include "VM/HktVMInterpreter.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"

static EHktInsightsVMState ToInsightsVMState(EVMStatus Status)
{
    switch (Status)
    {
    case EVMStatus::Running:
    case EVMStatus::Ready:        return EHktInsightsVMState::Running;
    case EVMStatus::Yielded:
    case EVMStatus::WaitingEvent: return EHktInsightsVMState::Blocked;
    case EVMStatus::Completed:    return EHktInsightsVMState::Completed;
    case EVMStatus::Failed:       return EHktInsightsVMState::Error;
    default:                      return EHktInsightsVMState::Running;
    }
}
#endif

// ============================================================================
// 1. Entity Arrange System
// ============================================================================

void FHktEntityArrangeSystem::Process(FHktWorldState& WorldState, const TArray<int64>& RemovedOwnerIds)
{
    if (RemovedOwnerIds.Num() == 0)
        return;

    ScratchRemoveList.Reset();

    WorldState.ForEachEntity([&](FHktEntityId Id, int32 /*Slot*/)
    {
        int32 OwnerHash = WorldState.GetProperty(Id, PropertyId::OwnerPlayerHash);
        for (int64 RemovedId : RemovedOwnerIds)
        {
            if (static_cast<int64>(OwnerHash) == RemovedId)
            {
                ScratchRemoveList.Add(Id);
                break;
            }
        }
    });

    for (FHktEntityId Id : ScratchRemoveList)
    {
        WorldState.RemoveEntity(Id);
    }
}

// ============================================================================
// 2. VM Build System
// ============================================================================

void FHktVMBuildSystem::Process(
    const TArray<FHktEvent>& Events,
    int32 CurrentFrame,
    FHktVMRuntimePool& Pool,
    TArray<FHktVMHandle>& OutActiveVMs,
    FHktWorldState& WorldState)
{
    for (const FHktEvent& Event : Events)
    {
        const FHktVMProgram* Program = FHktVMProgramRegistry::Get().FindProgram(Event.EventTag);
        if (!Program)
        {
            UE_LOG(LogTemp, Warning, TEXT("VM Build: No program for %s"), *Event.EventTag.ToString());
            continue;
        }

        FHktVMHandle Handle = Pool.Allocate();
        if (!Handle.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("VM Build: Pool exhausted"));
            continue;
        }

        FHktVMRuntime* Runtime = Pool.Get(Handle);
        FHktVMContext* Context = Pool.GetContext(Handle);
        check(Runtime && Context);

        Context->WorldState = &WorldState;
        Context->SourceEntity = Event.SourceEntity;
        Context->TargetEntity = Event.TargetEntity;

        Runtime->Program = Program;
        Runtime->Context = Context;
        Runtime->PC = 0;
        Runtime->Status = EVMStatus::Ready;
        Runtime->CreationFrame = CurrentFrame;
        Runtime->WaitFrames = 0;
        Runtime->EventWait.Reset();
        Runtime->SpatialQuery.Reset();
        FMemory::Memzero(Runtime->Registers, sizeof(Runtime->Registers));

        Runtime->SetRegEntity(Reg::Self, Event.SourceEntity);
        Runtime->SetRegEntity(Reg::Target, Event.TargetEntity);

        Context->Write(PropertyId::Param0, Event.Param0);
        Context->Write(PropertyId::Param1, Event.Param1);
        Context->Write(PropertyId::TargetPosX, FMath::RoundToInt(Event.Location.X));
        Context->Write(PropertyId::TargetPosY, FMath::RoundToInt(Event.Location.Y));
        Context->Write(PropertyId::TargetPosZ, FMath::RoundToInt(Event.Location.Z));

        OutActiveVMs.Add(Handle);
        WorldState.ActiveEvents.Add(Event);

#if WITH_HKT_INSIGHTS
        Runtime->SourceEventId = Event.EventId;
        FHktInsightsDataCollector::Get().RecordIntentEvent(
            Event.EventId, Event.EventTag, Event.SourceEntity, Event.TargetEntity,
            Event.Location, EHktInsightsEventState::Processing);
        FHktInsightsDataCollector::Get().RecordVMCreated(
            static_cast<int32>(Handle.Index), Event.EventId, Event.EventTag,
            Program->CodeSize(), Event.SourceEntity);
#endif

        UE_LOG(LogTemp, Log, TEXT("VM created: %s for Entity %d"), *Event.EventTag.ToString(), Event.SourceEntity);
    }
}

// ============================================================================
// 3. VM Process System
// ============================================================================

void FHktVMProcessSystem::Process(
    TArray<FHktVMHandle>& ActiveVMs,
    TArray<FHktVMHandle>& OutCompletedVMs,
    FHktVMRuntimePool& Pool,
    float DeltaSeconds,
    TArray<FHktPendingEvent>& PendingExternalEvents)
{
    ScratchEvents.Reset();
    Swap(ScratchEvents, PendingExternalEvents);

    Pool.ForEachActive([&](FHktVMHandle Handle, FHktVMRuntime& Runtime)
    {
        if (Runtime.Status == EVMStatus::WaitingEvent)
        {
            if (Runtime.EventWait.Type == EWaitEventType::Timer)
            {
                Runtime.EventWait.RemainingTime -= DeltaSeconds;
                if (Runtime.EventWait.RemainingTime <= 0.0f)
                {
                    Runtime.EventWait.Reset();
                    Runtime.Status = EVMStatus::Ready;
                }
            }
            else
            {
                for (int32 i = ScratchEvents.Num() - 1; i >= 0; --i)
                {
                    if (ScratchEvents[i].Type == Runtime.EventWait.Type &&
                        ScratchEvents[i].WatchedEntity == Runtime.EventWait.WatchedEntity)
                    {
                        if (ScratchEvents[i].Type == EWaitEventType::Collision)
                        {
                            Runtime.SetRegEntity(Reg::Hit, ScratchEvents[i].HitEntity);
                        }
                        Runtime.EventWait.Reset();
                        Runtime.Status = EVMStatus::Ready;
                        ScratchEvents.RemoveAtSwap(i);
                        break;
                    }
                }
            }
        }

        if (Runtime.Status == EVMStatus::Yielded)
        {
            if (Runtime.WaitFrames <= 0)
            {
                Runtime.Status = EVMStatus::Ready;
            }
            else
            {
                Runtime.WaitFrames--;
            }
        }
    });

    for (int32 i = ActiveVMs.Num() - 1; i >= 0; --i)
    {
        FHktVMHandle Handle = ActiveVMs[i];
        FHktVMRuntime* Runtime = Pool.Get(Handle);
        if (!Runtime)
        {
            ActiveVMs.RemoveAtSwap(i);
            continue;
        }

        if (!Runtime->IsRunnable())
            continue;

        Runtime->Status = EVMStatus::Running;
        EVMStatus Result = Interpreter->Execute(*Runtime);
        Runtime->Status = Result;

#if WITH_HKT_INSIGHTS
        {
            FString OpName;
            if (Runtime->Program && Runtime->PC > 0 && Runtime->Program->Code.Num() > 0)
            {
                int32 Idx = FMath::Min(Runtime->PC - 1, Runtime->Program->Code.Num() - 1);
                OpName = FString::Printf(TEXT("Op%d"), static_cast<uint8>(Runtime->Program->Code[Idx].GetOpCode()));
            }
            FHktInsightsDataCollector::Get().RecordVMTick(
                static_cast<int32>(Handle.Index), Runtime->PC, ToInsightsVMState(Result), OpName);
        }
#endif

        if (Result == EVMStatus::Completed || Result == EVMStatus::Failed)
        {
#if WITH_HKT_INSIGHTS
            FHktInsightsDataCollector::Get().RecordVMCompleted(
                static_cast<int32>(Handle.Index), Result == EVMStatus::Completed);
            FHktInsightsDataCollector::Get().UpdateIntentEventState(
                Runtime->SourceEventId,
                Result == EVMStatus::Completed ? EHktInsightsEventState::Completed : EHktInsightsEventState::Failed);
#endif
            OutCompletedVMs.Add(Handle);
            ActiveVMs.RemoveAtSwap(i);
        }
    }
}

// ============================================================================
// 4. Physics System
// ============================================================================

FHktPhysicsSystem::FCellCoord FHktPhysicsSystem::WorldToCell(const FVector& Pos)
{
    FCellCoord Coord;
    Coord.X = FMath::FloorToInt(Pos.X / CellSize);
    Coord.Y = FMath::FloorToInt(Pos.Y / CellSize);
    return Coord;
}

void FHktPhysicsSystem::RebuildGrid(const FHktWorldState& WorldState)
{
    GridMap.Reset();
    WorldState.ForEachEntity([&](FHktEntityId Id, int32 /*Slot*/)
    {
        FVector Pos;
        Pos.X = static_cast<float>(WorldState.GetProperty(Id, PropertyId::PosX));
        Pos.Y = static_cast<float>(WorldState.GetProperty(Id, PropertyId::PosY));
        Pos.Z = 0.f;
        FCellCoord Cell = WorldToCell(Pos);
        GridMap.FindOrAdd(Cell).Add(Id);
    });
}

void FHktPhysicsSystem::Process(
    FHktWorldState& WorldState,
    TArray<FHktPhysicsEvent>& OutPhysicsEvents)
{
    OutPhysicsEvents.Reset();
    RebuildGrid(WorldState);

    static constexpr float CollisionRadius = 50.0f;
    static constexpr float CollisionRadiusSq = CollisionRadius * CollisionRadius;

    for (auto& CellPair : GridMap)
    {
        const TArray<FHktEntityId>& EntitiesInCell = CellPair.Value;
        for (int32 i = 0; i < EntitiesInCell.Num(); ++i)
        {
            for (int32 j = i + 1; j < EntitiesInCell.Num(); ++j)
            {
                FHktEntityId A = EntitiesInCell[i];
                FHktEntityId B = EntitiesInCell[j];

                if (!WorldState.IsValidEntity(A) || !WorldState.IsValidEntity(B))
                    continue;

                FVector PosA(
                    static_cast<float>(WorldState.GetProperty(A, PropertyId::PosX)),
                    static_cast<float>(WorldState.GetProperty(A, PropertyId::PosY)),
                    static_cast<float>(WorldState.GetProperty(A, PropertyId::PosZ)));
                FVector PosB(
                    static_cast<float>(WorldState.GetProperty(B, PropertyId::PosX)),
                    static_cast<float>(WorldState.GetProperty(B, PropertyId::PosY)),
                    static_cast<float>(WorldState.GetProperty(B, PropertyId::PosZ)));

                float DistSq = FVector::DistSquared(PosA, PosB);
                if (DistSq <= CollisionRadiusSq)
                {
                    FHktPhysicsEvent PhysEvent;
                    PhysEvent.EntityA = A;
                    PhysEvent.EntityB = B;
                    PhysEvent.ContactPoint = (PosA + PosB) * 0.5f;
                    OutPhysicsEvents.Add(PhysEvent);
                }
            }
        }
    }
}

// ============================================================================
// 5. VM Cleanup System
// ============================================================================

void FHktVMCleanupSystem::Process(TArray<FHktVMHandle>& CompletedVMs, FHktVMRuntimePool& Pool, FHktWorldState& WorldState)
{
    for (FHktVMHandle Handle : CompletedVMs)
    {
        FHktVMRuntime* Runtime = Pool.Get(Handle);
        if (Runtime)
        {
            UE_LOG(LogTemp, Log, TEXT("VM finalized: %s"),
                Runtime->Program ? *Runtime->Program->Tag.ToString() : TEXT("unknown"));

            if (Runtime->Program && Runtime->Context)
            {
                FGameplayTag Tag = Runtime->Program->Tag;
                FHktEntityId Source = Runtime->Context->SourceEntity;
                WorldState.ActiveEvents.RemoveAll([&](const FHktEvent& E)
                {
                    return E.SourceEntity == Source && E.EventTag == Tag;
                });
            }

            if (Runtime->Context)
            {
                Runtime->Context->Reset();
            }
        }
        Pool.Free(Handle);
    }
    CompletedVMs.Reset();
}
