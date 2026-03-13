// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMWorldStateProxy.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktVMWorldStateProxy
// ============================================================================

void FHktVMWorldStateProxy::Initialize(const FHktWorldState& WS)
{
    constexpr int32 Reserve = 2176;
    DirtyMask.Reserve(Reserve);
    DirtySlots.Reserve(256);
    TagsDirtyMask.Reserve(Reserve);
    TagsDirtySlots.Reserve(256);
    PreFrameData.Reserve(Reserve * FHktWorldState::Stride);
    PreFrameTagContainers.Reserve(Reserve);
    PreFrameOwnerUids.Reserve(Reserve);
    OwnerDirtyMask.Reserve(Reserve);
    OwnerDirtySlots.Reserve(256);
}

void FHktVMWorldStateProxy::ResetDirtyIndices(const FHktWorldState& WS)
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

    if (WS.ActiveCount > 0)
    {
        PreFrameData = WS.Data;
        PreFrameTagContainers = WS.TagContainers;
        PreFrameOwnerUids = WS.OwnerUids;
    }
}

void FHktVMWorldStateProxy::SetPropertyDirty(FHktWorldState& WS, FHktEntityId Entity, uint16 PropId, int32 Value)
{
    if (!WS.IsValidEntity(Entity)) return;
    int32 Slot = WS.GetSlot(Entity);
    SetDirty(WS, Slot, PropId, Value);
}

void FHktVMWorldStateProxy::SetOwnerUid(FHktWorldState& WS, FHktEntityId Entity, int64 Uid)
{
    if (!WS.IsValidEntity(Entity)) return;
    int32 Slot = WS.GetSlot(Entity);
    SetOwnerDirty(WS, Slot, Uid);
}
