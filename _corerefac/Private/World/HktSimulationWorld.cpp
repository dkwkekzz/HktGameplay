// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "World/HktSimulationWorld.h"

FHktSimulationWorld::FHktSimulationWorld()
{
}

FHktSimulationWorld::~FHktSimulationWorld()
{
    Shutdown();
}

void FHktSimulationWorld::Initialize()
{
    // SpatialSystem 초기화 (WorldState 참조)
    SpatialSystem.Initialize(&WorldState);

    // 충돌 이벤트 태그 캐싱 및 SpatialSystem에 전달
    CollisionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.System.OnCollision")), false);
    SpatialSystem.SetCollisionEventTag(CollisionEventTag);

    // WorldState → IHktMasterStashInterface 어댑터 생성
    WorldStateAdapter = MakeUnique<FHktWorldStateAdapter>(&WorldState, &SpatialSystem);

    // VMProcessor 생성 (기존 팩토리 함수 사용, IHktStashInterface로 전달)
    VMProcessor = CreateVMProcessor(WorldStateAdapter.Get());

    UE_LOG(LogTemp, Log, TEXT("[SimulationWorld] Initialized"));
}

void FHktSimulationWorld::Shutdown()
{
    // VMProcessor를 먼저 해제 (WorldState 참조 무효화 방지)
    VMProcessor.Reset();
    WorldStateAdapter.Reset();

    SpatialSystem.Shutdown();

    UE_LOG(LogTemp, Log, TEXT("[SimulationWorld] Shutdown"));
}

void FHktSimulationWorld::Tick(uint32 FrameNumber, float DeltaSeconds)
{
    // ================================================================
    // [Phase 1-A] Process Deferred SystemEvents from Last Frame
    // 지난 프레임에서 생성된 충돌 이벤트 등을 VM으로 처리
    // "Resolve Now, React Later" 패턴: 게임플레이 반응은 1프레임 지연
    // ================================================================
    if (VMProcessor && DeferredSystemEvents.Num() > 0)
    {
        VMProcessor->ProcessSystemEvents(
            DeferredSystemEvents,
            static_cast<int32>(FrameNumber),
            DeltaSeconds);
        DeferredSystemEvents.Empty();
    }

    // ================================================================
    // [Phase 1-B] Process Input (IntentEvent)
    // VMProcessor가 외부에서 수신된 FHktIntentEvent 큐를 모두 비움.
    // 이동, 스킬 사용 등의 1차적인 상태 변화가 WorldState에 반영.
    // ================================================================
    if (VMProcessor)
    {
        VMProcessor->Tick(static_cast<int32>(FrameNumber), DeltaSeconds);
    }

    // ================================================================
    // [Phase 2] Spatial Update & Collision Resolution
    // 위치 변경된 엔티티의 셀 갱신
    // 충돌 감지 + 즉시 depenetration + SystemEvent 생성
    // ================================================================
    SpatialSystem.UpdateEntityPositions();

    TArray<FHktSystemEvent> NewSystemEvents;
    SpatialSystem.ResolveOverlapsAndGenEvents(WorldState, NewSystemEvents);

    // ================================================================
    // [Phase 3] Queue for Next Frame
    // 생성된 SystemEvent를 다음 프레임 처리를 위해 저장
    // ================================================================
    DeferredSystemEvents = MoveTemp(NewSystemEvents);

    // 프레임 완료 마킹
    WorldState.MarkFrameCompleted(static_cast<int32>(FrameNumber));
}

void FHktSimulationWorld::AddInputEvent(const FHktIntentEvent& Event)
{
    if (VMProcessor)
    {
        VMProcessor->NotifyIntentEvent(Event);
    }
}
