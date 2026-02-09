// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationWorld.h"
#include "Containers/Array.h"

namespace Hkt
{
    FHktSimulationWorld::FHktSimulationWorld()
        : VMProcessor(&WorldState, &SpatialSystem)
    {
    }

    void FHktSimulationWorld::Tick(uint32 FrameNumber, double Time)
    {
        // [Phase 1-3] Simulation Logic
        VMProcessor.SwapQueues();
        VMProcessor.ProcessCurrentQueue();

        TArray<FHktEvent> CollisionEvents;
        SpatialSystem.ResolveCollisionsAndGenEvents(WorldState, CollisionEvents);

        for (const auto& Evt : CollisionEvents)
        {
            VMProcessor.EnqueueEvent(Evt);
        }

        // [Phase 4] Publish Snapshot (State -> Linear Snapshot)
        PublishSnapshot(FrameNumber, Time, CollisionEvents);
    }

    void FHktSimulationWorld::AddInputEvent(const FHktEvent& Event)
    {
        VMProcessor.EnqueueEvent(Event);
    }

    FHktFrameSnapshotConstPtr FHktSimulationWorld::GetLastSnapshot() const
    {
        FScopeLock Lock(&SnapshotLock);
        return LastCommittedSnapshot;
    }

    void FHktSimulationWorld::PublishSnapshot(uint32 FrameNumber, double Time, const TArray<FHktEvent>& CurrentEvents)
    {
        TSharedPtr<FHktFrameSnapshot, ESPMode::ThreadSafe> NewSnapshot = MakeShared<FHktFrameSnapshot, ESPMode::ThreadSafe>();

        NewSnapshot->FrameNumber = FrameNumber;
        NewSnapshot->Timestamp = Time;
        NewSnapshot->Events = CurrentEvents;

        // Optimization: Linearize TMap -> TArray
        const auto& AllEntities = WorldState.GetAllEntities();
        NewSnapshot->Entities.Reserve(AllEntities.Num());

        for (const auto& Pair : AllEntities)
        {
            NewSnapshot->Entities.Add({ Pair.Key, Pair.Value });
        }

        // [Optimization] Exchange Pattern to minimize Lock duration
        FHktFrameSnapshotConstPtr OldSnapshot;
        {
            FScopeLock Lock(&SnapshotLock);
            OldSnapshot = LastCommittedSnapshot;
            LastCommittedSnapshot = NewSnapshot;
        }
    }
}
