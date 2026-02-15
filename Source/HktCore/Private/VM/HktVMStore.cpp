// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMStore.h"

// ============================================================================
// FHktVMStore
// ============================================================================

int32 FHktVMStore::Read(uint16 PropertyId) const
{
    return ReadEntity(SourceEntity, PropertyId);
}

int32 FHktVMStore::ReadEntity(FHktEntityId Entity, uint16 PropertyId) const
{
    uint64 Key = MakeCacheKey(Entity, PropertyId);
    for (const FLocalCacheEntry& Entry : LocalCache)
    {
        if (Entry.Key == Key)
            return Entry.Value;
    }

    // WorldState SOA에서 읽기
    if (WorldState)
    {
        return WorldState->GetProperty(Entity, PropertyId);
    }
    return 0;
}

void FHktVMStore::Write(uint16 PropertyId, int32 Value)
{
    WriteEntity(SourceEntity, PropertyId, Value);
}

void FHktVMStore::WriteEntity(FHktEntityId Entity, uint16 PropertyId, int32 Value)
{
    uint64 Key = MakeCacheKey(Entity, PropertyId);

    // LocalCache 업데이트 (기존 항목 덮어쓰기 or 추가)
    bool bFound = false;
    for (FLocalCacheEntry& Entry : LocalCache)
    {
        if (Entry.Key == Key)
        {
            Entry.Value = Value;
            bFound = true;
            break;
        }
    }
    if (!bFound)
    {
        LocalCache.Add({ Key, Value });
    }

    PendingWrites.Add({ PropertyId, Entity, Value });
}

void FHktVMStore::ClearPendingWrites()
{
    PendingWrites.Reset();  // 용량 유지
}

void FHktVMStore::Reset()
{
    PendingWrites.Reset();  // 용량 유지
    LocalCache.Reset();     // 용량 유지
    SourceEntity = InvalidEntityId;
    TargetEntity = InvalidEntityId;
}
