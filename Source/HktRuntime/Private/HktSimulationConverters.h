// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktPropertyIds.h"
#include "HktRuntimeTypes.h"

/**
 * HktRuntime <-> HktCore 타입 변환 유틸리티
 * 
 * Runtime 모듈 내부에서만 사용 (양쪽 타입 모두 접근 가능)
 */
namespace HktSimulationConverters
{
    // ========================================================================
    // Runtime -> Core
    // ========================================================================

    /** FHktRuntimeEvent -> FHktEvent */
    inline FHktEvent ConvertEvent(const FHktRuntimeEvent& In)
    {
        FHktEvent Out;
        Out.EventTag     = In.EventTag;
        Out.SourceEntity = In.SourceEntityId;
        Out.TargetEntity = In.TargetEntityId;
        Out.Location     = In.Location;

        // Payload -> Param0/Param1 (처음 8바이트를 int32 쌍으로 해석)
        if (In.Payload.Num() >= 4)
        {
            Out.Param0 = *reinterpret_cast<const int32*>(In.Payload.GetData());
        }
        if (In.Payload.Num() >= 8)
        {
            Out.Param1 = *reinterpret_cast<const int32*>(In.Payload.GetData() + 4);
        }
        return Out;
    }

    /** FHktRuntimeBatch -> FHktSimulationEvent */
    inline FHktSimulationEvent ConvertBatch(const FHktRuntimeBatch& In)
    {
        FHktSimulationEvent Out;
        Out.FrameNumber    = In.FrameNumber;
        Out.RandomSeed     = In.RandomSeed;
        Out.DeltaSeconds   = In.DeltaSeconds;
        Out.RemovedOwnerIds = In.RemovedOwnerIds;

        Out.Events.Reserve(In.Events.Num());
        for (const FHktRuntimeEvent& E : In.Events)
        {
            Out.Events.Add(ConvertEvent(E));
        }
        return Out;
    }

    // ========================================================================
    // Core -> Runtime
    // ========================================================================

    /** FHktWorldState -> FHktRuntimeSimulationState */
    inline FHktRuntimeSimulationState ConvertWorldState(const FHktWorldState& In)
    {
        FHktRuntimeSimulationState Out;
        Out.LastProcessedFrameNumber = In.FrameNumber;

        Out.EntitySnapshots.Reserve(In.Entities.Num());
        for (const auto& Pair : In.Entities)
        {
            FHktEntitySnapshot Snap;
            Snap.EntityId   = Pair.Key;
            Snap.Properties = Pair.Value.Properties;
            Snap.Tags       = Pair.Value.Tags;
            Out.EntitySnapshots.Add(MoveTemp(Snap));
        }
        return Out;
    }

    // ========================================================================
    // Runtime -> Core (상태 복원)
    // ========================================================================

    /** FHktRuntimeSimulationState -> FHktWorldState */
    inline FHktWorldState ConvertToWorldState(const FHktRuntimeSimulationState& In)
    {
        FHktWorldState Out;
        Out.FrameNumber = In.LastProcessedFrameNumber;

        FHktEntityId MaxId = 0;
        for (const FHktEntitySnapshot& Snap : In.EntitySnapshots)
        {
            FHktEntityState& State = Out.Entities.Add(Snap.EntityId);
            State.EntityId   = Snap.EntityId;
            State.Properties = Snap.Properties;
            State.Tags       = Snap.Tags;

            // Position은 Properties에서 복원
            State.Position.X = static_cast<float>(State.GetProperty(PropertyId::PosX));
            State.Position.Y = static_cast<float>(State.GetProperty(PropertyId::PosY));
            State.Position.Z = static_cast<float>(State.GetProperty(PropertyId::PosZ));

            if (Snap.EntityId > MaxId)
            {
                MaxId = Snap.EntityId;
            }
        }

        // 다음 할당 ID = 스냅샷 내 최대 EntityId + 1
        Out.NextEntityId = (In.EntitySnapshots.Num() > 0) ? (MaxId + 1) : 0;
        return Out;
    }
}
