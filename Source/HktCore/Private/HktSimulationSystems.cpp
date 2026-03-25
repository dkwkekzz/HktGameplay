// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationSystems.h"
#include "HktCoreLog.h"
#include "HktCoreProperties.h"
#include "VM/HktVMProgram.h"
#include "VM/HktVMRuntime.h"
#include "VM/HktVMInterpreter.h"
#include "VM/HktVMWorldStateProxy.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/IConsoleManager.h"
#include "HktCoreEventLog.h"

// ============================================================================
// 콘솔 변수 (CVar) - 런타임 이동 조작감 튜닝용
// 사용법: 에디터 콘솔 창에서 "hkt.Move.AccelMultiplier 5.0" 등 입력
// ============================================================================

static TAutoConsoleVariable<float> CVarMoveAccelMultiplier(
    TEXT("hkt.Move.AccelMultiplier"),
    7.0f, // 기본적으로 기존 대비 3배 기민하게 가속/감속
    TEXT("Multiplier for movement acceleration/deceleration. Higher means snappier movement."),
    ECVF_Default);

static TAutoConsoleVariable<float> CVarMoveSlowingRadius(
    TEXT("hkt.Move.SlowingRadius"),
    150.0f, // 감속을 시작하는 반경 (기존 250 -> 150으로 줄여 더 늦게 감속 시작 = 더 빠른 도착)
    TEXT("Radius at which entities start to slow down when reaching target."),
    ECVF_Default);

static TAutoConsoleVariable<float> CVarMoveMinSpeed(
    TEXT("hkt.Move.MinSpeed"),
    50.0f, // 도착지 근처 멈칫거림을 방지하는 최소 보장 속도 (기존 30 -> 50)
    TEXT("Minimum speed enforced to prevent infinite arrival time (Zeno's paradox)."),
    ECVF_Default);


#if ENABLE_HKT_INSIGHTS
#include "HktCoreDataCollector.h"
#include "HktStoryTypes.h"

static FString VMStatusToString(EVMStatus Status)
{
    switch (Status)
    {
    case EVMStatus::Running:      return TEXT("Running");
    case EVMStatus::Ready:        return TEXT("Ready");
    case EVMStatus::Yielded:      return TEXT("Yielded");
    case EVMStatus::WaitingEvent: return TEXT("Blocked");
    case EVMStatus::Completed:    return TEXT("Completed");
    case EVMStatus::Failed:       return TEXT("Failed");
    default:                      return TEXT("Unknown");
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

    HKT_EVENT_LOG(HktLogTags::Core_Entity,
        FString::Printf(TEXT("EntityArrange: Removing %d entities for %d owners"),
            ScratchRemoveList.Num(), RemovedOwnerIds.Num()));
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
            UE_LOG(LogHktCore, Error, TEXT("VM Build: No program for %s — Story가 등록되지 않았습니다 (빌드 검증 실패 또는 미등록)"), *Event.EventTag.ToString());
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
                    HKT_EVENT_LOG_TAG(HktLogTags::Core_VM,
                        FString::Printf(TEXT("VM cancelled (duplicate): %s Entity=%d"),
                            *Event.EventTag.ToString(), Event.SourceEntity),
                        Event.SourceEntity, Event.EventTag);
                }
            }
        }

        FHktVMHandle Handle = Pool.Allocate();
        if (!Handle.IsValid())
        {
            UE_LOG(LogHktCore, Warning, TEXT("VM Build: Pool exhausted"));
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

        // 이벤트 파라미터를 Context 로컬에 저장 (SourceEntity 없이도 LoadStore로 읽기 가능)
        Context->EventParam0 = Event.Param0;
        Context->EventParam1 = Event.Param1;
        Context->EventTargetPosX = FMath::RoundToInt(Event.Location.X);
        Context->EventTargetPosY = FMath::RoundToInt(Event.Location.Y);
        Context->EventTargetPosZ = FMath::RoundToInt(Event.Location.Z);

        // 이벤트 태그를 SourceEntity에 자동 부여
        if (WorldState.IsValidEntity(Event.SourceEntity))
        {
            VMProxy.AddTag(WorldState, Event.SourceEntity, Event.EventTag);
        }

        OutActiveVMs.Add(Handle);
        WorldState.ActiveEvents.Add(Event);

#if ENABLE_HKT_INSIGHTS
        Runtime->SourceEventId = Event.EventId;
        {
            FString VMKey = FString::Printf(TEXT("VM_%d"), static_cast<int32>(Handle.Index));
            HKT_INSIGHT_COLLECT(TEXT("VM"), VMKey,
                FString::Printf(TEXT("Created | Event=%s | Src=%d | Tgt=%d | CodeSize=%d | Source=%s"),
                    *Event.EventTag.ToString(), Event.SourceEntity, Event.TargetEntity,
                    Program->CodeSize(), *InsightsSource));

            FString IntentKey = FString::Printf(TEXT("Intent_%d"), Event.EventId);
            HKT_INSIGHT_COLLECT(TEXT("VM"), IntentKey,
                FString::Printf(TEXT("Processing | Tag=%s | Src=%d | Tgt=%d"),
                    *Event.EventTag.ToString(), Event.SourceEntity, Event.TargetEntity));
        }
#endif

        HKT_EVENT_LOG_TAG(HktLogTags::Core_VM,
            FString::Printf(TEXT("VM created: %s Src=%d Tgt=%d CodeSize=%d"),
                *Event.EventTag.ToString(), Event.SourceEntity, Event.TargetEntity,
                Program->CodeSize()),
            Event.SourceEntity, Event.EventTag);
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

#if ENABLE_HKT_INSIGHTS
        {
            FString OpName;
            if (Runtime->Program && Runtime->PC > 0 && Runtime->Program->Code.Num() > 0)
            {
                int32 Idx = FMath::Min(Runtime->PC - 1, Runtime->Program->Code.Num() - 1);
                OpName = GetOpCodeName(Runtime->Program->Code[Idx].GetOpCode());
            }
            FString VMKey = FString::Printf(TEXT("VM_%d"), static_cast<int32>(Handle.Index));
            HKT_INSIGHT_COLLECT(TEXT("VM"), VMKey,
                FString::Printf(TEXT("%s | PC=%d | Op=%s | Src=%d"),
                    *VMStatusToString(Result), Runtime->PC, *OpName, Runtime->Context ? Runtime->Context->SourceEntity : -1));
        }
#endif

        if (Result == EVMStatus::Completed || Result == EVMStatus::Failed)
        {
            if (Result == EVMStatus::Failed)
            {
                UE_LOG(LogHktCore, Error, TEXT("VM FAILED: %s Src=%d PC=%d — client sent invalid intent"),
                    Runtime->Program ? *Runtime->Program->Tag.ToString() : TEXT("?"),
                    Runtime->Context ? Runtime->Context->SourceEntity : InvalidEntityId,
                    Runtime->PC);
            }
            HKT_EVENT_LOG_ENTITY_EX(HktLogTags::Core_VM,
                Result == EVMStatus::Failed ? EHktLogLevel::Error : EHktLogLevel::Info,
                EHktLogSource::Core,
                FString::Printf(TEXT("VM %s: %s PC=%d"),
                    Result == EVMStatus::Completed ? TEXT("Completed") : TEXT("Failed"),
                    Runtime->Program ? *Runtime->Program->Tag.ToString() : TEXT("?"),
                    Runtime->PC),
                Runtime->Context ? Runtime->Context->SourceEntity : InvalidEntityId);
#if ENABLE_HKT_INSIGHTS
            {
                FString VMKey = FString::Printf(TEXT("VM_%d"), static_cast<int32>(Handle.Index));
                HKT_INSIGHT_COLLECT(TEXT("VM"), VMKey,
                    FString::Printf(TEXT("%s | PC=%d"),
                        Result == EVMStatus::Completed ? TEXT("Completed") : TEXT("Failed"), Runtime->PC));

                FString IntentKey = FString::Printf(TEXT("Intent_%d"), Runtime->SourceEventId);
                HKT_INSIGHT_COLLECT(TEXT("VM"), IntentKey,
                    FString::Printf(TEXT("%s"),
                        Result == EVMStatus::Completed ? TEXT("Completed") : TEXT("Failed")));
            }
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

    // 콘솔 변수 조회 (엔티티를 순회하는 루프 진입 전 1회만 캐싱하여 퍼포먼스 확보)
    const float AccelMultiplier = CVarMoveAccelMultiplier.GetValueOnAnyThread();
    const float SlowingRadius = CVarMoveSlowingRadius.GetValueOnAnyThread();
    const float MinSpeed = CVarMoveMinSpeed.GetValueOnAnyThread();

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

        // 회전(Yaw) 연산 및 미세 거리 각도 튐 방지
        if (DistSq > 1.0f) // 거리가 1cm 이상일 때만 방향을 갱신 (부동소수점 오차로 인한 떨림 방지)
        {
            const int32 YawDeg = FMath::RoundToInt(FMath::Atan2(DY, DX) * (180.0f / PI));
            VMProxy.SetPropertyDirty(WorldState, Id, PropertyId::RotYaw, YawDeg);
        }

        // 도착 판정
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

        // 2. 목표 속력(Desired Speed) 계산: 반경 내에 들어오면 CVar 기반으로 감속
        float DesiredSpeed = MaxSpeed;
        if (Dist < SlowingRadius)
        {
            // 정수 반올림 함정에 빠지지 않도록 CVar 기반의 최소 보정 속도 강제
            DesiredSpeed = FMath::Max(MaxSpeed * (Dist / SlowingRadius), MinSpeed);
        }

        // 3. 속력 가감속 적용 (가속도에 CVar 배율 보정치 적용하여 더욱 기민한 움직임 보장)
        const float Accel = (Force / Mass) * AccelMultiplier;
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

void FHktVMCleanupSystem::Process(TArray<FHktVMHandle>& CompletedVMs, FHktVMRuntimePool& Pool, FHktWorldState& WorldState, FHktVMWorldStateProxy& VMProxy)
{
    for (FHktVMHandle Handle : CompletedVMs)
    {
        FHktVMRuntime* Runtime = Pool.Get(Handle);
        if (Runtime)
        {
            HKT_EVENT_LOG_ENTITY(HktLogTags::Core_VM,
                FString::Printf(TEXT("VM finalized: %s"),
                    Runtime->Program ? *Runtime->Program->Tag.ToString() : TEXT("unknown")),
                Runtime->Context ? Runtime->Context->SourceEntity : InvalidEntityId);

            if (Runtime->Program && Runtime->Context)
            {
                FGameplayTag Tag = Runtime->Program->Tag;
                FHktEntityId Source = Runtime->Context->SourceEntity;
                WorldState.ActiveEvents.RemoveAll([&](const FHktEvent& E)
                {
                    return E.SourceEntity == Source && E.EventTag == Tag;
                });

                // 이벤트 태그 + 자식 태그를 SourceEntity에서 일괄 제거
                if (WorldState.IsValidEntity(Source))
                {
                    int32 Slot = WorldState.GetSlot(Source);
                    const FGameplayTagContainer& CurrentTags = WorldState.GetTagsBySlot(Slot);
                    TArray<FGameplayTag> TagsToRemove;
                    for (const FGameplayTag& T : CurrentTags)
                    {
                        if (T.MatchesTag(Tag))
                        {
                            TagsToRemove.Add(T);
                        }
                    }
                    for (const FGameplayTag& T : TagsToRemove)
                    {
                        VMProxy.RemoveTag(WorldState, Source, T);
                    }
                }
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