// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMWorldStateProxy.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktVMEntityPoolProxy
// ============================================================================

void FHktVMEntityPoolProxy::Initialize(int32 Reserve)
{
    DirtyMask.Reserve(Reserve);
    DirtySlots.Reserve(256);
    TagsDirtyMask.Reserve(Reserve);
    TagsDirtySlots.Reserve(256);
    PreFrameData.Reserve(Reserve * FHktEntityPool::Stride);
    PreFrameTagContainers.Reserve(Reserve);
    PreFrameOwnerUids.Reserve(Reserve);
    OwnerDirtyMask.Reserve(Reserve);
    OwnerDirtySlots.Reserve(256);
}

void FHktVMEntityPoolProxy::ResetDirty(const FHktEntityPool& Pool)
{
    for (int32 S : DirtySlots)
        if (S < DirtyMask.Num()) DirtyMask[S] = 0;
    for (int32 S : TagsDirtySlots)
        if (S < TagsDirtyMask.Num()) TagsDirtyMask[S] = 0;
    for (int32 S : OwnerDirtySlots)
        if (S < OwnerDirtyMask.Num()) OwnerDirtyMask[S] = 0;
    DirtySlots.Reset();
    TagsDirtySlots.Reset();
    OwnerDirtySlots.Reset();

    if (Pool.ActiveCount > 0)
    {
        PreFrameData = Pool.Data;
        PreFrameTagContainers = Pool.TagContainers;
        PreFrameOwnerUids = Pool.OwnerUids;
    }
}

// ============================================================================
// FHktVMWorldStateProxy
// ============================================================================

void FHktVMWorldStateProxy::Initialize(const FHktWorldState& WS)
{
    PoolProxy.Initialize(2176);
}

void FHktVMWorldStateProxy::ResetDirtyIndices(const FHktWorldState& WS)
{
    PoolProxy.ResetDirty(WS.Pool);
}

void FHktVMWorldStateProxy::SetPropertyDirty(FHktWorldState& WS, FHktEntityId Entity, uint16 PropId, int32 Value)
{
    if (!WS.IsValidEntity(Entity)) return;
    int32 Slot = WS.GetSlot(Entity);
    PoolProxy.SetDirty(WS.Pool, Slot, PropId, Value);
}

void FHktVMWorldStateProxy::SetOwnerUid(FHktWorldState& WS, FHktEntityId Entity, int64 Uid)
{
    if (!WS.IsValidEntity(Entity)) return;
    int32 Slot = WS.GetSlot(Entity);
    PoolProxy.SetOwnerDirty(WS.Pool, Slot, Uid);
}

void FHktVMWorldStateProxy::AddTag(FHktWorldState& WS, FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!WS.IsValidEntity(Entity)) return;
    int32 Slot = WS.GetSlot(Entity);
    PoolProxy.AddTag(WS.Pool, Slot, Tag);
}

void FHktVMWorldStateProxy::RemoveTag(FHktWorldState& WS, FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!WS.IsValidEntity(Entity)) return;
    int32 Slot = WS.GetSlot(Entity);
    PoolProxy.RemoveTag(WS.Pool, Slot, Tag);
}
