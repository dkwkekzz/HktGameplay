// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationSystems.h"
#include "HktCoreProperties.h"
#include "VM/HktVMProgram.h"
#include "VM/HktVMRuntime.h"
#include "VM/HktVMInterpreter.h"
#include "VM/HktVMWorldStateProxy.h"
#include "Math/UnrealMathUtility.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#include "HktFlowTypes.h"

static EHktInsightsVMState ToInsightsVMState(EVMStatus Status)
{
    switch (Status)
    {
    case EVMStatus::Running:
    case EVMStatus::Ready:        return EHktInsightsVMState::Running;
    case EVMStatus::Yielded:      return EHktInsightsVMState::Yielded;
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

    for (int64 RemovedId : RemovedOwnerIds)
    {
        WorldState.ForEachEntityByOwner(RemovedId, [&](FHktEntityId Id, int32 /*Slot*/)
        {
            ScratchRemoveList.Add(Id);
        });
    }

    for (FHktEntityId Id : ScratchRemoveList)
        WorldState.RemoveEntity(Id);
}

// ============================================================================
// 2. VM Build System
// ============================================================================

void FHktVMBuildSystem::Process(
    const TArray<FHktEvent>& Events,
    int32 CurrentFrame,
    FHktVMRuntimePool& Pool,
    TArray<FHktVMHandle>& OutActiveVMs,
    FHktWorldState& WorldState,
    FHktVMWorldStateProxy& VMProxy,
    const FString& InsightsSource)
{
    for (const FHktEvent& Event : Events)
    {
        const FHktVMProgram* Program = FHktVMProgramRegistry::Get().FindProgram(Event.EventTag);
        if (!Program)
        {
            UE_LOG(LogTemp, Warning, TEXT("VM Build: No program for %s"), *Event.EventTag.ToString());
            continue;
        }

        // CancelOnDuplicate: 같은 EventTag + SourceEntity의 기존 VM 취소
        if (Program->bCancelOnDuplicate)
        {
            for (int32 i = OutActiveVMs.Num() - 1; i >= 0; --i)
            {
                FHktVMRuntime* Existing = Pool.Get(OutActiveVMs[i]);
                if (Existing && Existing->Program && Existing->Context
                    && Existing->Program->Tag == Event.EventTag
                    && Existing->Context->SourceEntity == Event.SourceEntity)
                {
                    Existing->Status = EVMStatus::Completed;
                    UE_LOG(LogTemp, Log, TEXT("VM cancelled (duplicate): %s for Entity %d"),
                        *Event.EventTag.ToString(), Event.SourceEntity);
                }
            }
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
        Context->VMProxy = &VMProxy;
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

        Runtime->PlayerUid = Event.PlayerUid;
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
            Program->CodeSize(), Event.SourceEntity, InsightsSource);
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
        {
            // 외부에서 취소된 VM (VMBuildSystem 중복 이벤트 취소 등) 정리
            if (Runtime->IsTerminated())
            {
                OutCompletedVMs.Add(Handle);
                ActiveVMs.RemoveAtSwap(i);
            }
            continue;
        }

        Runtime->Status = EVMStatus::Running;
        EVMStatus Result = Interpreter->Execute(*Runtime);
        Runtime->Status = Result;

#if WITH_HKT_INSIGHTS
        {
            FString OpName;
            if (Runtime->Program && Runtime->PC > 0 && Runtime->Program->Code.Num() > 0)
            {
                int32 Idx = FMath::Min(Runtime->PC - 1, Runtime->Program->Code.Num() - 1);
                OpName = GetOpCodeName(Runtime->Program->Code[Idx].GetOpCode());
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
// 3.5 Movement System
// ============================================================================

void FHktMovementSystem::Process(
    FHktWorldState& WorldState,
    FHktVMWorldStateProxy& VMProxy,
    TArray<FHktPendingEvent>& OutMoveEndEvents)
{
    OutMoveEndEvents.Reset();

    static constexpr float ArrivalThresholdSq = 16.0f;  // 4cm (도착 판정)
    static constexpr float SlowingRadius = 250.0f;      // 감속 시작 거리 (2.5m)

    WorldState.ForEachEntity([&](FHktEntityId Id, int32 /*Slot*/)
    {
        if (WorldState.GetProperty(Id, PropertyId::IsMoving) == 0)
            return;

        const float CurX = static_cast<float>(WorldState.GetProperty(Id, PropertyId::PosX));
        const float CurY = static_cast<float>(WorldState.GetProperty(Id, PropertyId::PosY));
        const float CurZ = static_cast<float>(WorldState.GetProperty(Id, PropertyId::PosZ));

        const float TgtX = static_cast<float>(WorldState.GetProperty(Id, PropertyId::MoveTargetX));
        const float TgtY = static_cast<float>(WorldState.GetProperty(Id, PropertyId::MoveTargetY));
        const float TgtZ = static_cast<float>(WorldState.GetProperty(Id, PropertyId::MoveTargetZ));

        const float Force = static_cast<float>(WorldState.GetProperty(Id, PropertyId::MoveForce));
        const float Mass = static_cast<float>(FMath::Max(WorldState.GetProperty(Id, PropertyId::Mass), 1));

        // 현재 속도 읽기
        float VX = static_cast<float>(WorldState.GetProperty(Id, PropertyId::VelX));
        float VY = static_cast<float>(WorldState.GetProperty(Id, PropertyId::VelY));
        float VZ = static_cast<float>(WorldState.GetProperty(Id, PropertyId::VelZ));

        // 방향 계산
        const float DX = TgtX - CurX;
        const float DY = TgtY - CurY;
        const float DZ = TgtZ - CurZ;
        const float DistSq = DX * DX + DY * DY + DZ * DZ;

        // [수정점] 회전(Yaw) 연산을 최상단으로 이동 및 미세 거리 각도 튐 방지
        if (DistSq > 1.0f) // 거리가 1cm 이상일 때만 방향을 갱신 (부동소수점 오차로 인한 떨림 방지)
        {
            const int32 YawDeg = FMath::RoundToInt(FMath::Atan2(DY, DX) * (180.0f / PI));
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::RotYaw, YawDeg);
        }

        // 도착 판정 (기존 로직 유지)
        if (DistSq <= ArrivalThresholdSq)
        {
            VMProxy.SetPosition(WorldState, Id,
                FMath::RoundToInt(TgtX), FMath::RoundToInt(TgtY), FMath::RoundToInt(TgtZ));
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelX, 0);
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelY, 0);
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelZ, 0);
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::IsMoving, 0);

            FHktPendingEvent Evt;
            Evt.Type = EWaitEventType::MoveEnd;
            Evt.WatchedEntity = Id;
            OutMoveEndEvents.Add(Evt);
            return;
        }

        const float Dist = FMath::Sqrt(DistSq);
        const float InvDist = 1.0f / Dist;
        const float DirX = DX * InvDist;
        const float DirY = DY * InvDist;
        const float DirZ = DZ * InvDist;

        // 1. 현재 속력(Speed) 계산 (방향과 속력을 분리)
        float CurSpeed = FMath::Sqrt(VX * VX + VY * VY + VZ * VZ);

        // 2. 목표 속력(Desired Speed) 계산: 반경 내에 들어오면 속도를 선형으로 줄임
        float DesiredSpeed = MaxSpeed;
        if (Dist < SlowingRadius)
        {
            DesiredSpeed = MaxSpeed * (Dist / SlowingRadius);
        }

        // 3. 속력 가감속 적용 (가속도로 속력 크기만 변경하여 관성은 유지하되 방향은 즉시 전환)
        const float Accel = Force / Mass;
        const float MaxSpeedChange = Accel * FixedDeltaSeconds;

        if (CurSpeed < DesiredSpeed)
        {
            CurSpeed = FMath::Min(CurSpeed + MaxSpeedChange, DesiredSpeed);
        }
        else if (CurSpeed > DesiredSpeed)
        {
            CurSpeed = FMath::Max(CurSpeed - MaxSpeedChange, DesiredSpeed);
        }

        // 이번 프레임에 이동할 실제 거리
        const float MoveStep = CurSpeed * FixedDeltaSeconds;

        // 4. 오버슈트 방지: 이번 프레임의 이동 거리가 남은 거리와 같거나 크면 즉시 스냅 처리
        if (MoveStep >= Dist)
        {
            VMProxy.SetPosition(WorldState, Id,
                FMath::RoundToInt(TgtX), FMath::RoundToInt(TgtY), FMath::RoundToInt(TgtZ));
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelX, 0);
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelY, 0);
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelZ, 0);
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::IsMoving, 0);

            FHktPendingEvent Evt;
            Evt.Type = EWaitEventType::MoveEnd;
            Evt.WatchedEntity = Id;
            OutMoveEndEvents.Add(Evt);
            return;
        }

        // 5. 직선 궤적으로 속도 벡터 재설정 (곡선 제거, 항상 타겟을 향함)
        VX = DirX * CurSpeed;
        VY = DirY * CurSpeed;
        VZ = DirZ * CurSpeed;

        float NewX = CurX + VX * FixedDeltaSeconds;
        float NewY = CurY + VY * FixedDeltaSeconds;
        float NewZ = CurZ + VZ * FixedDeltaSeconds;

        VMProxy.SetPosition(WorldState, Id,
            FMath::RoundToInt(NewX), FMath::RoundToInt(NewY), FMath::RoundToInt(NewZ));
        VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelX, FMath::RoundToInt(VX));
        VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelY, FMath::RoundToInt(VY));
        VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::VelZ, FMath::RoundToInt(VZ));
    });
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
        FIntVector P = WorldState.GetPosition(Id);
        FVector Pos(static_cast<float>(P.X), static_cast<float>(P.Y), 0.f);
        FCellCoord Cell = WorldToCell(Pos);
        GridMap.FindOrAdd(Cell).Add(Id);
    });
}

void FHktPhysicsSystem::Process(
    FHktWorldState& WorldState,
    FHktVMWorldStateProxy& VMProxy,
    TArray<FHktPhysicsEvent>& OutPhysicsEvents)
{
    OutPhysicsEvents.Reset();
    RebuildGrid(WorldState);

    static constexpr float DefaultCollisionRadius = 50.0f;

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

                FIntVector PA = WorldState.GetPosition(A);
                FIntVector PB = WorldState.GetPosition(B);
                FVector PosA(static_cast<float>(PA.X), static_cast<float>(PA.Y), static_cast<float>(PA.Z));
                FVector PosB(static_cast<float>(PB.X), static_cast<float>(PB.Y), static_cast<float>(PB.Z));

                const float RadiusA = FMath::Max(static_cast<float>(WorldState.GetProperty(A, PropertyId::CollisionRadius)), DefaultCollisionRadius);
                const float RadiusB = FMath::Max(static_cast<float>(WorldState.GetProperty(B, PropertyId::CollisionRadius)), DefaultCollisionRadius);
                const float CombinedRadius = RadiusA + RadiusB;

                const float DistSq = FVector::DistSquared(PosA, PosB);
                if (DistSq <= CombinedRadius * CombinedRadius)
                {
                    // 충돌 이벤트
                    FHktPhysicsEvent PhysEvent;
                    PhysEvent.EntityA = A;
                    PhysEvent.EntityB = B;
                    PhysEvent.ContactPoint = (PosA + PosB) * 0.5f;
                    OutPhysicsEvents.Add(PhysEvent);

                    // Push-out 위치 보정
                    const float Dist = FMath::Sqrt(DistSq);
                    if (Dist > SMALL_NUMBER)
                    {
                        const float Overlap = CombinedRadius - Dist;
                        const float HalfPush = Overlap * 0.5f;
                        const FVector Dir = (PosB - PosA) / Dist;

                        const FVector NewA = PosA - Dir * HalfPush;
                        const FVector NewB = PosB + Dir * HalfPush;

                        VMProxy.SetPosition(WorldState, A,
                            FMath::RoundToInt(NewA.X), FMath::RoundToInt(NewA.Y), FMath::RoundToInt(NewA.Z));
                        VMProxy.SetPosition(WorldState, B,
                            FMath::RoundToInt(NewB.X), FMath::RoundToInt(NewB.Y), FMath::RoundToInt(NewB.Z));
                    }
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