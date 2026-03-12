// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMWorldStateProxy.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktVMEntityPoolProxy
// ============================================================================

void FHktVMEntityPoolProxy::Initialize(const FHktEntityPool& Pool, int32 Reserve)
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

    // Pre-frame 스냅샷 (UndoDiff OldValue 조회용)
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
    PoolProxies[HktType::Unit].Initialize(WS.GetPool(HktType::Unit), 512);
    PoolProxies[HktType::Projectile].Initialize(WS.GetPool(HktType::Projectile), 1024);
    PoolProxies[HktType::Equipment].Initialize(WS.GetPool(HktType::Equipment), 512);
    PoolProxies[HktType::Building].Initialize(WS.GetPool(HktType::Building), 128);
}

void FHktVMWorldStateProxy::ResetDirtyIndices(const FHktWorldState& WS)
{
    for (int32 T = 1; T < HktType::MaxTypes; ++T)
        PoolProxies[T].ResetDirty(WS.GetPool(static_cast<FHktTypeId>(T)));
}

void FHktVMWorldStateProxy::SetPropertyDirty(FHktWorldState& WS, FHktEntityId Entity, uint16 PropId, int32 Value)
{
    if (!WS.IsValidEntity(Entity)) return;
    const FHktWorldState::FEntityLocation& L = WS.EntityLocations[Entity];
    FHktEntityPool& Pool = WS.Pools[L.TypeId];
    PoolProxies[L.TypeId].SetDirty(Pool, L.PoolSlot, PropId, Value);
}

void FHktVMWorldStateProxy::SetOwnerUid(FHktWorldState& WS, FHktEntityId Entity, int64 Uid)
{
    if (!WS.IsValidEntity(Entity)) return;
    const FHktWorldState::FEntityLocation& L = WS.EntityLocations[Entity];
    PoolProxies[L.TypeId].SetOwnerDirty(WS.Pools[L.TypeId], L.PoolSlot, Uid);
}

void FHktVMWorldStateProxy::AddTag(FHktWorldState& WS, FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!WS.IsValidEntity(Entity)) return;
    const FHktWorldState::FEntityLocation& L = WS.EntityLocations[Entity];
    PoolProxies[L.TypeId].AddTag(WS.Pools[L.TypeId], L.PoolSlot, Tag);
}

void FHktVMWorldStateProxy::RemoveTag(FHktWorldState& WS, FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!WS.IsValidEntity(Entity)) return;
    const FHktWorldState::FEntityLocation& L = WS.EntityLocations[Entity];
    PoolProxies[L.TypeId].RemoveTag(WS.Pools[L.TypeId], L.PoolSlot, Tag);
}
