// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClientSimulatorComponent.h"
#include "HktPropertyIds.h"

UHktClientSimulatorComponent::UHktClientSimulatorComponent() 
{ 
    PrimaryComponentTick.bCanEverTick = false; 
}

void UHktClientSimulatorComponent::BeginPlay() 
{ 
    Super::BeginPlay();

    // CoreSimulator 초기화 (BeginPlay에서 생성)
    CoreSimulator = CreateSimulationWorld();
}

void UHktClientSimulatorComponent::Execute(const FHktSimulationEvent& InBatch)
{
    if (!CoreSimulator)
    {
        return;
    }

    // Runtime 타입 -> Core 타입 변환
    // 결정론적 시뮬레이션 실행
    CoreSimulator->ProcessBatch(InBatch);

    // Core 상태 -> Runtime 상태로 역변환하여 캐시
    CoreSimulator->SnapshotWorldState(State);

    bInitialized = true;
}

void UHktClientSimulatorComponent::RestoreState(const FHktWorldState& InState, TArray<FHktSimulationEvent>&& InPendingBatches)
{
    if (!CoreSimulator)
    {
        return;
    }

    CoreSimulator->RestoreWorldState(InState);

    // 복원 이후 프레임만 필터링
    TArray<FHktSimulationEvent> ReplayBatches;
    for (FHktSimulationEvent& Event : InPendingBatches)
    {
        if (Event.FrameNumber > InState.FrameNumber)
        {
            ReplayBatches.Add(MoveTemp(Event));
        }
    }

    // 복수 배치 일괄 실행 (중간 스냅샷 없음)
    if (ReplayBatches.Num() > 0)
    {
        CoreSimulator->ProcessBatches(ReplayBatches);
    }

    // 최종 상태만 한 번 스냅샷
    CoreSimulator->SnapshotWorldState(State);
    bInitialized = true;
}
