// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "HktEntityTypes.h"
#include "HktEventTypes.h"
#include "HktWorldState.h"
#include "HktLocalContext.h"
#include "HktSpatialSystem.h"

namespace Hkt
{
    // ==================================================================================
    // VM Processor - VM 코어 & 더블 버퍼링
    // ==================================================================================

    struct FHktExecutionState
    {
        uint32 ProgramCounter;
        uint8  Registers[256];
        uint8  StackMemory[1024];
        int32  StackPointer;

        void Reset();
    };

    class HKTCORE_API FHktVMProcessor
    {
    public:
        FHktVMProcessor(FHktWorldState* InWorldState, FHktSpatialSystem* InSpatialSystem);

        void EnqueueEvent(const FHktEvent& Event);
        void SwapQueues();
        void ProcessCurrentQueue();

    private:
        void ExecuteInternal(const FHktEvent& Event);
        void RunExecutionLoop(const FHktEvent& Event, FHktLocalContext& Context);

    private:
        FHktWorldState* WorldState;
        FHktSpatialSystem* SpatialSystem;
        FHktExecutionState CoreState;

        TArray<FHktEvent> CurrentQueue;
        TArray<FHktEvent> NextFrameQueue;
    };
}
