// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerSimulator.h"

void FHktServerSimulator::Execute(const FHktFrameBatch& InBatch)
{
    State.LastProcessedFrameNumber = InBatch.FrameNumber;
    // TODO: EventTag 기반 시뮬레이션 로직
    bInitialized = true;
}

void FHktServerSimulator::RestoreState(const FHktGroupSimulationState& InState, TArray<FHktFrameBatch>&& InPendingBatches)
{
    State = InState;
    bInitialized = true;
    for (const FHktFrameBatch& Batch : InPendingBatches) { Execute(Batch); }
}

FHktOwnerSimulationState FHktServerSimulator::GetOwnerSimulationState(int64 InOwnerId) const
{
    FHktOwnerSimulationState OwnerState;
    return OwnerState;
}