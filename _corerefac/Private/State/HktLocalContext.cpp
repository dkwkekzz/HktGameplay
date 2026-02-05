// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "State/HktLocalContext.h"
#include "State/HktWorldState.h"

// ============================================================================
// FHktLocalContext
// ============================================================================

int32 FHktLocalContext::Read(uint16 PropertyId) const
{
    return ReadEntity(SourceEntity, PropertyId);
}

int32 FHktLocalContext::ReadEntity(FHktEntityId Entity, uint16 PropertyId) const
{
    uint64 Key = MakeCacheKey(Entity, PropertyId);
    if (const int32* Cached = LocalCache.Find(Key))
    {
        return *Cached;
    }

    return WorldState ? WorldState->GetProperty(Entity, PropertyId) : 0;
}

void FHktLocalContext::Write(uint16 PropertyId, int32 Value)
{
    WriteEntity(SourceEntity, PropertyId, Value);
}

void FHktLocalContext::WriteEntity(FHktEntityId Entity, uint16 PropertyId, int32 Value)
{
    uint64 Key = MakeCacheKey(Entity, PropertyId);
    LocalCache.Add(Key, Value);

    FPendingWrite W;
    W.Entity = Entity;
    W.PropertyId = PropertyId;
    W.Value = Value;
    PendingWrites.Add(W);
}

void FHktLocalContext::CommitChanges()
{
    if (!WorldState)
        return;

    // PendingWrites를 WorldState::FPendingWrite 형식으로 변환하여 일괄 적용
    TArray<FHktWorldState::FPendingWrite> WorldWrites;
    WorldWrites.Reserve(PendingWrites.Num());
    for (const auto& W : PendingWrites)
    {
        FHktWorldState::FPendingWrite WW;
        WW.Entity = W.Entity;
        WW.PropertyId = W.PropertyId;
        WW.Value = W.Value;
        WorldWrites.Add(WW);
    }

    WorldState->ApplyWrites(WorldWrites);
    ClearPendingWrites();
}

void FHktLocalContext::Rollback()
{
    ClearPendingWrites();
}

void FHktLocalContext::ClearPendingWrites()
{
    PendingWrites.Reset();
    LocalCache.Reset();
}

void FHktLocalContext::Reset()
{
    PendingWrites.Reset();
    LocalCache.Reset();
    SourceEntity = InvalidEntityId;
    TargetEntity = InvalidEntityId;
}
