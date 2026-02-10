// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClientSimulatorComponent.h"

UHktClientSimulatorComponent::UHktClientSimulatorComponent() 
{ 
    PrimaryComponentTick.bCanEverTick = false; 
}

void UHktClientSimulatorComponent::BeginPlay() 
{ 
    Super::BeginPlay();
}

void UHktClientSimulatorComponent::Execute(const FHktFrameBatch& InBatch)
{
    State.LastProcessedFrameNumber = InBatch.FrameNumber;
    // TODO: 서버와 동일한 결정론적 시뮬레이션
    bInitialized = true;
}

void UHktClientSimulatorComponent::RestoreState(const FHktGroupSimulationState& InState, TArray<FHktFrameBatch>&& InPendingBatches)
{
    State = InState;
    bInitialized = true;
    for (const FHktFrameBatch& Batch : InPendingBatches)
    {
        if (Batch.FrameNumber > State.LastProcessedFrameNumber) { Execute(Batch); }
    }
}

FHktOwnerSimulationState UHktClientSimulatorComponent::GetOwnerSimulationState(int64 InOwnerId) const
{
    FHktOwnerSimulationState OwnerState;
    return OwnerState;
}
