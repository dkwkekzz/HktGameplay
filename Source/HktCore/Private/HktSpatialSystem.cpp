// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSpatialSystem.h"
#include "Math/UnrealMathSSE.h"

namespace Hkt
{
    FHktSpatialSystem::FHktSpatialSystem()
        : CellSize(1000.0f)
    {
    }

    FCellCoord FHktSpatialSystem::WorldToCell(const FVector3& Pos) const
    {
        return { FMath::FloorToInt(Pos.X / CellSize), FMath::FloorToInt(Pos.Y / CellSize) };
    }

    void FHktSpatialSystem::UpdateEntityPosition(FEntityID EntityID, const FVector3& OldPos, const FVector3& NewPos)
    {
        FCellCoord OldCell = WorldToCell(OldPos);
        FCellCoord NewCell = WorldToCell(NewPos);
        if (!(OldCell == NewCell))
        {
            if (TArray<FEntityID>* OldGrid = GridMap.Find(OldCell))
            {
                OldGrid->Remove(EntityID);
                if (OldGrid->IsEmpty()) GridMap.Remove(OldCell);
            }
            GridMap.FindOrAdd(NewCell).Add(EntityID);
        }
    }

    void FHktSpatialSystem::ResolveCollisionsAndGenEvents(FHktWorldState& State, TArray<FHktEvent>& OutEvents)
    {
        for (auto& Pair : GridMap)
        {
            const TArray<FEntityID>& Entities = Pair.Value;
            for (int32 i = 0; i < Entities.Num(); ++i)
            {
                for (int32 j = i + 1; j < Entities.Num(); ++j)
                {
                    FEntityID A = Entities[i];
                    FEntityID B = Entities[j];
                    ResolveOverlap(State, A, B, OutEvents);
                }
            }
        }
    }

    void FHktSpatialSystem::ResolveOverlap(FHktWorldState& State, FEntityID A, FEntityID B, TArray<FHktEvent>& OutEvents)
    {
        FComponentData* DataA = State.GetComponentMutable(A);
        FComponentData* DataB = State.GetComponentMutable(B);

        if (!DataA || !DataB) return;

        FHktEvent HitEvent;
        HitEvent.EventTag = (uint32)EEventTag::CollisionHit;
        HitEvent.Source = A;
        HitEvent.Target = B;
        HitEvent.bIsInternal = true;
        OutEvents.Add(HitEvent);
    }
}
