// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktWorldState.h"
#include "HktCoreProperties.h"
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

    /** 이벤트 파라미터 로컬 저장 — SourceEntity 없이도 LoadStore로 읽기 가능 */
    int32 EventTargetPosX = 0;
    int32 EventTargetPosY = 0;
    int32 EventTargetPosZ = 0;
    int32 EventParam0 = 0;
    int32 EventParam1 = 0;
    int32 EventParam2 = 0;
    int32 EventParam3 = 0;

    FORCEINLINE int32 Read(uint16 PropId) const
    {
        // 이벤트 파라미터는 로컬 저장소에서 읽기 (SourceEntity 불필요)
        switch (PropId)
        {
        case PropertyId::TargetPosX: return EventTargetPosX;
        case PropertyId::TargetPosY: return EventTargetPosY;
        case PropertyId::TargetPosZ: return EventTargetPosZ;
        case PropertyId::Param0:     return EventParam0;
        case PropertyId::Param1:     return EventParam1;
        case PropertyId::Param2:     return EventParam2;
        case PropertyId::Param3:     return EventParam3;
        default: break;
        }
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
        EventTargetPosX = EventTargetPosY = EventTargetPosZ = 0;
        EventParam0 = EventParam1 = EventParam2 = EventParam3 = 0;
    }
};
