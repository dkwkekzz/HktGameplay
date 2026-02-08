// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HktEntityTypes.h"
#include "HktEventTypes.h"
#include "HktWorldState.h"

namespace Hkt
{
    // ==================================================================================
    // Physics & Spatial - 물리 및 공간 관리
    // ==================================================================================

    class HKTCORE_API FHktSpatialSystem
    {
    public:
        FHktSpatialSystem();

        FCellCoord WorldToCell(const FVector3& Pos) const;
        void UpdateEntityPosition(FEntityID EntityID, const FVector3& OldPos, const FVector3& NewPos);
        void ResolveCollisionsAndGenEvents(FHktWorldState& State, TArray<FHktEvent>& OutEvents);

    private:
        void ResolveOverlap(FHktWorldState& State, FEntityID A, FEntityID B, TArray<FHktEvent>& OutEvents);

    private:
        TMap<FCellCoord, TArray<FEntityID>> GridMap;
        float CellSize;
    };
}
