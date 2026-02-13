// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationSystems.h"
#include "HktPropertyIds.h"
#include "VM/HktVMProgram.h"
#include "VM/HktVMRuntime.h"
#include "VM/HktVMStore.h"
#include "VM/HktVMInterpreter.h"

// ============================================================================
// 1. Entity Arrange System
// ============================================================================

void FHktEntityArrangeSystem::Process(FHktWorldState& WorldState, const TArray<int64>& RemovedOwnerIds)
{
    if (RemovedOwnerIds.Num() == 0)
        return;

    // 제거된 소유자에 속하는 엔티티를 찾아서 삭제
    TArray<FHktEntityId> EntitiesToRemove;

    const FHktDataColumn* OwnerCol = WorldState.GetColumn(PropertyId::OwnerPlayerHash);
    if (!OwnerCol)
        return;

    WorldState.ForEachEntity([&](FHktEntityId Id, int32 SlotIndex)
    {
        int32 OwnerHash = OwnerCol->Get(SlotIndex);
        for (int64 RemovedId : RemovedOwnerIds)
        {
            if (static_cast<int64>(OwnerHash) == RemovedId)
            {
                EntitiesToRemove.Add(Id);
                break;
            }
        }
    });

    for (FHktEntityId Id : EntitiesToRemove)
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
    FHktWorldState& WorldState,
    TArray<FHktVMStore>& StorePool)
{
    for (const FHktEvent& Event : Events)
    {
        // 소스 엔티티가 유효한지 확인
        if (!WorldState.IsValidEntity(Event.SourceEntity))
        {
            UE_LOG(LogTemp, Warning, TEXT("VM Build: SourceEntity %d not valid"), Event.SourceEntity);
            continue;
        }

        // 프로그램 찾기
        const FHktVMProgram* Program = FHktVMProgramRegistry::Get().FindProgram(Event.EventTag);
        if (!Program)
        {
            UE_LOG(LogTemp, Warning, TEXT("VM Build: No program for %s"), *Event.EventTag.ToString());
            continue;
        }

        // VM 할당
        FHktVMHandle Handle = Pool.Allocate();
        if (!Handle.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("VM Build: Pool exhausted"));
            continue;
        }

        FHktVMRuntime* Runtime = Pool.Get(Handle);
        check(Runtime);

        // Store 할당 및 초기화
        FHktVMStore& Store = StorePool[Handle.Index];
        Store.WorldState = &WorldState;
        Store.SourceEntity = Event.SourceEntity;
        Store.TargetEntity = Event.TargetEntity;
        Store.ClearPendingWrites();
        Store.LocalCache.Reset();

        // Runtime 초기화
        Runtime->Program = Program;
        Runtime->Store = &Store;
        Runtime->PC = 0;
        Runtime->Status = EVMStatus::Ready;
        Runtime->CreationFrame = CurrentFrame;
        Runtime->WaitFrames = 0;
        Runtime->EventWait.Reset();
        Runtime->SpatialQuery.Reset();
        FMemory::Memzero(Runtime->Registers, sizeof(Runtime->Registers));

        // 레지스터 초기화: Self, Target
        Runtime->SetRegEntity(Reg::Self, Event.SourceEntity);
        Runtime->SetRegEntity(Reg::Target, Event.TargetEntity);

        // 이벤트 파라미터를 Store에 기록
        Store.Write(PropertyId::Param0, Event.Param0);
        Store.Write(PropertyId::Param1, Event.Param1);

        // 타겟 위치 설정
        Store.Write(PropertyId::TargetPosX, FMath::RoundToInt(Event.Location.X));
        Store.Write(PropertyId::TargetPosY, FMath::RoundToInt(Event.Location.Y));
        Store.Write(PropertyId::TargetPosZ, FMath::RoundToInt(Event.Location.Z));

        OutActiveVMs.Add(Handle);
        WorldState.ActiveEvents.Add(Event);

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
    // 외부 이벤트를 로컬로 이동 (순회 중 새 이벤트 추가 방지)
    TArray<FHktPendingEvent> ExternalEvents = MoveTemp(PendingExternalEvents);
    PendingExternalEvents.Reset();

    // 단일 ForEachActive — 이벤트 매칭 + 상태 전환
    Pool.ForEachActive([&](FHktVMHandle Handle, FHktVMRuntime& Runtime)
    {
        // 1) WaitingEvent VM: 외부 이벤트 매칭 또는 타이머 업데이트
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
                for (int32 i = ExternalEvents.Num() - 1; i >= 0; --i)
                {
                    if (ExternalEvents[i].Type == Runtime.EventWait.Type &&
                        ExternalEvents[i].WatchedEntity == Runtime.EventWait.WatchedEntity)
                    {
                        if (ExternalEvents[i].Type == EWaitEventType::Collision)
                        {
                            Runtime.SetRegEntity(Reg::Hit, ExternalEvents[i].HitEntity);
                        }
                        Runtime.EventWait.Reset();
                        Runtime.Status = EVMStatus::Ready;
                        ExternalEvents.RemoveAtSwap(i);
                        break;
                    }
                }
            }
        }

        // 2) Yielded VM: 프레임 카운트다운
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

    // VM 실행
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

        if (Result == EVMStatus::Completed || Result == EVMStatus::Failed)
        {
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
    const FHktDataColumn* ColX = WorldState.GetColumn(PropertyId::PosX);
    const FHktDataColumn* ColY = WorldState.GetColumn(PropertyId::PosY);

    WorldState.ForEachEntity([&](FHktEntityId Id, int32 SlotIndex)
    {
        FVector Pos;
        Pos.X = ColX ? static_cast<float>(ColX->Get(SlotIndex)) : 0.f;
        Pos.Y = ColY ? static_cast<float>(ColY->Get(SlotIndex)) : 0.f;
        // Z는 2D 그리드에서 불필요
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

    // 컬럼 포인터 사전 캐싱 (루프 내 TMap 룩업 제거)
    const FHktDataColumn* ColX = WorldState.GetColumn(PropertyId::PosX);
    const FHktDataColumn* ColY = WorldState.GetColumn(PropertyId::PosY);
    const FHktDataColumn* ColZ = WorldState.GetColumn(PropertyId::PosZ);

    // 간단한 충돌 감지: 같은 셀 내 엔티티 쌍 비교
    static constexpr float CollisionRadius = 50.0f; // 기본 충돌 반경 (cm)
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

                int32 IdxA = WorldState.EntityToIndex[A];
                int32 IdxB = WorldState.EntityToIndex[B];

                FVector PosA(
                    ColX ? static_cast<float>(ColX->Get(IdxA)) : 0.f,
                    ColY ? static_cast<float>(ColY->Get(IdxA)) : 0.f,
                    ColZ ? static_cast<float>(ColZ->Get(IdxA)) : 0.f);
                FVector PosB(
                    ColX ? static_cast<float>(ColX->Get(IdxB)) : 0.f,
                    ColY ? static_cast<float>(ColY->Get(IdxB)) : 0.f,
                    ColZ ? static_cast<float>(ColZ->Get(IdxB)) : 0.f);

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
// 5. Apply Store System
// ============================================================================

void FHktApplyStoreSystem::Process(
    FHktWorldState& WorldState,
    const TArray<FHktVMHandle>& ActiveVMs,
    FHktVMRuntimePool& Pool)
{
    // 모든 활성 VM의 PendingWrites를 WorldState SOA 컬럼에 배치 기록
    Pool.ForEachActive([&](FHktVMHandle Handle, FHktVMRuntime& Runtime)
    {
        if (!Runtime.Store)
            return;

        for (auto& Pair : Runtime.Store->PendingWritesByProperty)
        {
            uint16 PropId = Pair.Key;
            FHktDataColumn& Col = WorldState.GetOrCreateColumn(static_cast<int32>(PropId));
            for (const FHktVMStore::FPendingWrite& W : Pair.Value)
            {
                if (WorldState.IsValidEntity(W.Entity))
                {
                    int32 Idx = WorldState.EntityToIndex[W.Entity];
                    Col.Set(Idx, W.Value);
                }
            }
        }
        Runtime.Store->ClearPendingWrites();
    });
}

// ============================================================================
// 6. VM Cleanup System
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

            // ActiveEvents에서 제거 (SourceEntity + EventTag 매칭)
            if (Runtime->Program && Runtime->Store)
            {
                FGameplayTag Tag = Runtime->Program->Tag;
                FHktEntityId Source = Runtime->Store->SourceEntity;
                WorldState.ActiveEvents.RemoveAll([&](const FHktEvent& E)
                {
                    return E.SourceEntity == Source && E.EventTag == Tag;
                });
            }

            if (Runtime->Store)
            {
                Runtime->Store->Reset();
            }
        }
        Pool.Free(Handle);
    }
    CompletedVMs.Reset();
}

// ============================================================================
// 7. Publish Render System
// ============================================================================

void FHktPublishRenderSystem::Process(const FHktWorldState& WorldState, FHktRenderState& OutRenderState)
{
    OutRenderState.FrameNumber = WorldState.FrameNumber;

    int32 EntityCount = WorldState.GetEntityCount();
    OutRenderState.EntityIds.Reset(EntityCount);
    OutRenderState.Positions.Reset(EntityCount);
    OutRenderState.Rotations.Reset(EntityCount);

    const FHktDataColumn* ColX = WorldState.GetColumn(PropertyId::PosX);
    const FHktDataColumn* ColY = WorldState.GetColumn(PropertyId::PosY);
    const FHktDataColumn* ColZ = WorldState.GetColumn(PropertyId::PosZ);
    const FHktDataColumn* ColYaw = WorldState.GetColumn(PropertyId::RotYaw);

    WorldState.ForEachEntity([&](FHktEntityId Id, int32 SlotIndex)
    {
        OutRenderState.EntityIds.Add(Id);

        FVector Pos(
            ColX ? static_cast<float>(ColX->Get(SlotIndex)) : 0.f,
            ColY ? static_cast<float>(ColY->Get(SlotIndex)) : 0.f,
            ColZ ? static_cast<float>(ColZ->Get(SlotIndex)) : 0.f);
        OutRenderState.Positions.Add(Pos);

        FRotator Rot(0.f, ColYaw ? static_cast<float>(ColYaw->Get(SlotIndex)) : 0.f, 0.f);
        OutRenderState.Rotations.Add(Rot);
    });
}
