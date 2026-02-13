// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerSimulator.h"
#include "HktPropertyIds.h"

FHktServerSimulator::FHktServerSimulator()
{
    CoreSimulator = CreateSimulationWorld();
}

void FHktServerSimulator::Execute(const FHktSimulationEvent& InBatch)
{
    // Runtime 타입 -> Core 타입 변환
    CoreSimulator->ProcessBatch(InBatch);

    State = CoreSimulator->GetWorldState();

    bInitialized = true;
}

FHktRuntimeOwnerState FHktServerSimulator::GetOwnerState(int64 InOwnerId) const
{
    FHktRuntimeOwnerState OwnerState;

    const FHktWorldState& WorldState = CoreSimulator->GetWorldState();
    for (const auto& Pair : WorldState.Entities)
    {
        // OwnerPlayerHash가 일치하는 엔티티만 포함
        if (Pair.Value.GetProperty(PropertyId::OwnerPlayerHash) == static_cast<int32>(InOwnerId))
        {
            OwnerState.EntityStates.Add(Pair.Value);
        }
    }

    return OwnerState;
}
