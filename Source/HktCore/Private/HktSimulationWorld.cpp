// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationWorld.h"
#include "Containers/Array.h"

namespace Hkt
{
    FHktSimulationWorld::FHktSimulationWorld()
        : VMProcessor(&WorldState, &SpatialSystem)
    {
    }

    void FHktSimulationWorld::Tick(uint32 FrameNumber)
    {
        VMProcessor.SwapQueues();
        VMProcessor.ProcessCurrentQueue();

        TArray<FHktEvent> CollisionEvents;
        SpatialSystem.ResolveCollisionsAndGenEvents(WorldState, CollisionEvents);

        for (const auto& Evt : CollisionEvents)
        {
            VMProcessor.EnqueueEvent(Evt);
        }
    }

    void FHktSimulationWorld::AddInputEvent(const FHktEvent& Event)
    {
        VMProcessor.EnqueueEvent(Event);
    }

    const FHktWorldState& FHktSimulationWorld::GetState() const
    {
        return WorldState;
    }
}
