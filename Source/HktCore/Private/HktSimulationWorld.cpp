// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationWorld.h"
#include "VM/HktVMRuntime.h"
#include "VM/HktVMInterpreter.h"
#include "VM/HktVMStore.h"

// PIMPL: Private 타입을 포함하는 내부 데이터
struct FHktSimWorldInternalData
{
    TArray<FHktVMStore> StorePool;

    void Initialize(int32 MaxVMs, const FHktWorldState* WorldState)
    {
        StorePool.SetNum(MaxVMs);
        for (FHktVMStore& Store : StorePool)
        {
            Store.WorldState = WorldState;
        }
    }
};

FHktSimulationWorld::FHktSimulationWorld()
{
    // Internal Data 생성
    InternalData = MakeUnique<FHktSimWorldInternalData>();

    // VM Pool 생성
    VMPool = MakeUnique<FHktVMRuntimePool>();

    // Interpreter 생성 및 WorldState 연결
    Interpreter = MakeUnique<FHktVMInterpreter>();
    Interpreter->Initialize(&WorldState);

    // VMProcessSystem에 Interpreter 연결
    VMProcessSystem.Interpreter = Interpreter.Get();

    // Store 풀 초기화
    InternalData->Initialize(256, &WorldState);
}

FHktSimulationWorld::~FHktSimulationWorld()
{
    // TUniquePtr가 자동 해제
}

void FHktSimulationWorld::ProcessBatch(const FHktSimulationEvent& Event)
{
    // 프레임 데이터 업데이트
    WorldState.FrameNumber = Event.FrameNumber;
    WorldState.RandomSeed = Event.RandomSeed;

    // ============================
    // Phase 1: 준비 (Preparation)
    // ============================

    // 1-1. 삭제된 소유자 정리
    EntityArrangeSystem.Process(WorldState, Event.RemovedOwnerIds);

    // 1-2. 이벤트 → VM 생성
    VMBuildSystem.Process(Event.Events, static_cast<int32>(Event.FrameNumber), *VMPool, ActiveVMs, WorldState, InternalData->StorePool);

    // ============================
    // Phase 2: 실행 (Execution)
    // ============================

    VMProcessSystem.Process(ActiveVMs, CompletedVMs, *VMPool, Event.DeltaSeconds, PendingExternalEvents);

    // ============================
    // Phase 3: 물리 및 적용 (Physics & Commit)
    // ============================

    // 3-1. 물리 충돌 감지
    PhysicsSystem.Process(WorldState, GeneratedPhysicsEvents);

    // 3-2. PhysicsEvent → PendingExternalEvents 변환
    //       다음 프레임 ProcessSystem에서 WaitCollision VM과 매칭
    for (const FHktPhysicsEvent& PhysEvent : GeneratedPhysicsEvents)
    {
        // A→B, B→A 양방향 등록 (어느 쪽이 WaitCollision 중인지 모르므로)
        FHktPendingEvent PendA;
        PendA.Type = EWaitEventType::Collision;
        PendA.WatchedEntity = PhysEvent.EntityA;
        PendA.HitEntity = PhysEvent.EntityB;
        PendingExternalEvents.Add(PendA);

        FHktPendingEvent PendB;
        PendB.Type = EWaitEventType::Collision;
        PendB.WatchedEntity = PhysEvent.EntityB;
        PendB.HitEntity = PhysEvent.EntityA;
        PendingExternalEvents.Add(PendB);
    }

    // 3-3. Store 변경사항을 WorldState에 커밋
    ApplyStoreSystem.Process(WorldState, ActiveVMs, *VMPool);

    // ============================
    // Phase 4: 정리 (Cleanup)
    // ============================

    VMCleanupSystem.Process(CompletedVMs, *VMPool, WorldState);
}

void FHktSimulationWorld::RestoreWorldState(const FHktWorldState& InState)
{
    WorldState.CopyFrom(InState);

    // VM 풀 리셋 (롤백 시 모든 VM 무효화)
    VMPool->Reset();
    ActiveVMs.Reset();
    CompletedVMs.Reset();
    GeneratedPhysicsEvents.Reset();
    PendingExternalEvents.Reset();

    // Interpreter의 WorldState 참조 갱신 (주소가 변하지 않으므로 불필요하지만 안전을 위해)
    Interpreter->Initialize(&WorldState);

    // Store 풀 WorldState 참조 갱신
    InternalData->Initialize(256, &WorldState);

    // ActiveEvents에 남아있는 진행 중 이벤트로 VM 재생성
    // (CopyFrom으로 ActiveEvents가 복원되었지만 대응하는 VM이 없으므로)
    if (WorldState.ActiveEvents.Num() > 0)
    {
        TArray<FHktEvent> EventsToRestore = WorldState.ActiveEvents;
        WorldState.ActiveEvents.Reset();
        VMBuildSystem.Process(EventsToRestore, static_cast<int32>(WorldState.FrameNumber), *VMPool, ActiveVMs, WorldState, InternalData->StorePool);
    }
}

void FHktSimulationWorld::SnapshotWorldState(FHktWorldState& OutState) const
{
    OutState.CopyFrom(WorldState);
}

void FHktSimulationWorld::CreateWorldView(FHktWorldView& OutView)
{
    PublishViewSystem.Process(WorldState, ActiveVMs, *VMPool, OutView);
}

// ============================================================================
// 팩토리 함수
// ============================================================================

TUniquePtr<IHktSimulator> CreateSimulationWorld()
{
    return MakeUnique<FHktSimulationWorld>();
}

