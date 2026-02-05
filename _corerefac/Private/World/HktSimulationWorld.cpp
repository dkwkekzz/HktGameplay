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

    // WorldState → IHktMasterStashInterface 어댑터 생성
    WorldStateAdapter = MakeUnique<FHktWorldStateAdapter>(&WorldState, &SpatialSystem);

    // VMProcessor 생성 (기존 팩토리 함수 사용, IHktStashInterface로 전달)
    VMProcessor = CreateVMProcessor(WorldStateAdapter.Get());

    // 충돌 이벤트 태그 캐싱
    CollisionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.System.OnCollision")), false);

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
    // [Phase 1] Input Processing (Intent)
    // VMProcessor가 외부에서 수신된 FHktIntentEvent 큐를 모두 비움.
    // 이동, 스킬 사용 등의 1차적인 상태 변화가 WorldState에 반영.
    // ================================================================
    if (VMProcessor)
    {
        VMProcessor->Tick(static_cast<int32>(FrameNumber), DeltaSeconds);
    }

    // ================================================================
    // [Phase 2] Spatial Update & Collision Detection
    // 변경된 위치 정보를 SpatialSystem에 업데이트.
    // 충돌이 감지되면 FHktCollisionPair 리스트를 생성.
    // ================================================================
    SpatialSystem.UpdateEntityPositions();

    TArray<FHktCollisionPair> Collisions;
    SpatialSystem.DetectWatchedCollisions(Collisions);

    // ================================================================
    // [Phase 3] Reaction Processing (System)
    // 충돌 결과를 VMProcessor에 전달하여 즉시 처리.
    // 1프레임 지연 없이 충돌 반응(체력 감소, 넉백 등)이 확정.
    // ================================================================
    if (VMProcessor)
    {
        for (const FHktCollisionPair& Pair : Collisions)
        {
            // Watch 기반 충돌 → VMProcessor NotifyCollision 경로로 전달
            VMProcessor->NotifyCollision(Pair.EntityA, Pair.EntityB);
        }
    }

    // SystemEvent 기반 충돌 (새로운 경로)
    // Phase 2에서 ExecuteImmediate + FHktSystemEvent 경로 추가 예정
    // 현재는 Watch 기반만 지원

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
