// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktEntityTypes.h"
#include "HktEventTypes.h"
#include "HktWorldState.h"
#include "HktSpatialSystem.h"
#include "HktVMProcessor.h"

namespace Hkt
{
    // ==================================================================================
    // Simulation World - 메인 루프
    // ==================================================================================

    class HKTCORE_API FHktSimulationWorld
    {
    public:
        FHktSimulationWorld();

        void Tick(uint32 FrameNumber);
        void AddInputEvent(const FHktEvent& Event);
        const FHktWorldState& GetState() const;

    private:
        FHktSpatialSystem SpatialSystem;
        FHktWorldState WorldState;
        FHktVMProcessor VMProcessor;
    };
}
