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

    CoreSimulator->SnapshotWorldState(State);

    bInitialized = true;
}

FHktRuntimeOwnerState FHktServerSimulator::GetOwnerState(int64 InOwnerId) const
{
    FHktRuntimeOwnerState OwnerState;

    State.ForEachEntity([&](FHktEntityId Id, int32 SlotIndex)
    {
        if (State.GetProperty(Id, PropertyId::OwnerPlayerHash) == static_cast<int32>(InOwnerId))
        {
            OwnerState.EntityStates.Add(State.ExtractEntityState(Id));
        }
    });

    return OwnerState;
}
