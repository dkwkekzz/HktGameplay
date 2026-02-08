// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMProcessor.h"

namespace Hkt
{
    void FHktExecutionState::Reset()
    {
        ProgramCounter = 0;
        StackPointer = 0;
    }

    FHktVMProcessor::FHktVMProcessor(FHktWorldState* InWorldState, FHktSpatialSystem* InSpatialSystem)
        : WorldState(InWorldState)
        , SpatialSystem(InSpatialSystem)
    {
        CoreState.Reset();
    }

    void FHktVMProcessor::EnqueueEvent(const FHktEvent& Event)
    {
        NextFrameQueue.Add(Event);
    }

    void FHktVMProcessor::SwapQueues()
    {
        CurrentQueue = MoveTemp(NextFrameQueue);
        NextFrameQueue.Reset();
    }

    void FHktVMProcessor::ProcessCurrentQueue()
    {
        for (const FHktEvent& Event : CurrentQueue)
        {
            ExecuteInternal(Event);
        }
        CurrentQueue.Reset();
    }

    void FHktVMProcessor::ExecuteInternal(const FHktEvent& Event)
    {
        CoreState.Reset();
        FHktLocalContext Context(WorldState);
        RunExecutionLoop(Event, Context);
        Context.CommitChanges();
    }

    void FHktVMProcessor::RunExecutionLoop(const FHktEvent& Event, FHktLocalContext& Context)
    {
        if (Event.EventTag == (uint32)EEventTag::Move)
        {
            FComponentData Data = Context.Read(Event.Source);
            FVector3 OldPos = Data.Position;
            Context.Write(Event.Source, Data);
            SpatialSystem->UpdateEntityPosition(Event.Source, OldPos, Data.Position);
        }
    }
}
