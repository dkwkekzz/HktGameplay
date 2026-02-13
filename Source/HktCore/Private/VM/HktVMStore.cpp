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
    if (const int32* Cached = LocalCache.Find(Key))
    {
        return *Cached;
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
    LocalCache.Add(Key, Value);

    FPendingWrite W;
    W.Entity = Entity;
    W.Value = Value;
    PendingWritesByProperty.FindOrAdd(PropertyId).Add(W);
}

void FHktVMStore::ClearPendingWrites()
{
    PendingWritesByProperty.Reset();
}

void FHktVMStore::Reset()
{
    PendingWritesByProperty.Reset();
    LocalCache.Reset();
    SourceEntity = InvalidEntityId;
    TargetEntity = InvalidEntityId;
}
