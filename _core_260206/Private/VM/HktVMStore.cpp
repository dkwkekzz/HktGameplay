#include "HktVMStore.h"
#include "HktCoreInterfaces.h"

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
    
    return Stash ? Stash->GetProperty(Entity, PropertyId) : 0;
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
    W.PropertyId = PropertyId;
    W.Value = Value;
    PendingWrites.Add(W);
}

void FHktVMStore::ClearPendingWrites()
{
    PendingWrites.Reset();
}

void FHktVMStore::Reset()
{
    PendingWrites.Reset();
    LocalCache.Reset();
    SourceEntity = InvalidEntityId;
    TargetEntity = InvalidEntityId;
}