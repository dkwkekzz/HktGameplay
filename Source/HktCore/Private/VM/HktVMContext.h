// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktWorldState.h"
#include "HktVMWorldStateProxy.h"

/**
 * FHktVMContext — VM 실행 컨텍스트 (구 FHktVMStore 대체)
 *
 * WorldState에 직접 읽기/쓰기. 쓰기는 VMProxy 경유하여 Dirty 추적.
 */
struct FHktVMContext
{
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    FHktWorldState* WorldState = nullptr;
    FHktVMWorldStateProxy* VMProxy = nullptr;

    FORCEINLINE int32 Read(uint16 PropId) const
    {
        return WorldState ? WorldState->GetProperty(SourceEntity, PropId) : 0;
    }

    FORCEINLINE int32 ReadEntity(FHktEntityId Entity, uint16 PropId) const
    {
        return WorldState ? WorldState->GetProperty(Entity, PropId) : 0;
    }

    FORCEINLINE void Write(uint16 PropId, int32 Value)
    {
        if (VMProxy) VMProxy->SetPropertyDirty(*WorldState, SourceEntity, PropId, Value);
    }

    FORCEINLINE void WriteEntity(FHktEntityId Entity, uint16 PropId, int32 Value)
    {
        if (VMProxy) VMProxy->SetPropertyDirty(*WorldState, Entity, PropId, Value);
    }

    void Reset()
    {
        SourceEntity = InvalidEntityId;
        TargetEntity = InvalidEntityId;
    }
};
