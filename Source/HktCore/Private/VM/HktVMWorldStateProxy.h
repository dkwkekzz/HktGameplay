// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktWorldState.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktVMEntityPoolProxy — 풀별 VM 중간 데이터 (CorePrivate 전용)
//
// DirtyMask / DirtySlots / TagsDirtyMask / TagsDirtySlots — 프레임 내 변경 추적
// PreFrameData / PreFrameTagContainers — 프레임 시작 스냅샷 (UndoDiff OldValue 조회)
// ============================================================================

struct FHktVMEntityPoolProxy
{
    TArray<uint64> DirtyMask;
    TArray<int32>  DirtySlots;
    TArray<uint8>  TagsDirtyMask;
    TArray<int32>  TagsDirtySlots;
    TArray<int32>  PreFrameData;
    TArray<FGameplayTagContainer> PreFrameTagContainers;

    TArray<int64>  PreFrameOwnerUids;
    TArray<int32>  OwnerDirtySlots;
    TArray<uint8>  OwnerDirtyMask;

    void Initialize(const FHktEntityPool& Pool, int32 Reserve);

    FORCEINLINE void SetDirty(FHktEntityPool& Pool, int32 Slot, uint16 PropId, int32 V)
    {
        Pool.Data[Slot * FHktEntityPool::Stride + PropId] = V;
        if (Slot >= DirtyMask.Num())
        {
            DirtyMask.SetNum(Slot + 1, EAllowShrinking::No);
            TagsDirtyMask.SetNum(Slot + 1, EAllowShrinking::No);
        }
        if (DirtyMask[Slot] == 0) DirtySlots.Add(Slot);
        DirtyMask[Slot] |= (1ULL << PropId);
    }

    FORCEINLINE void SetTagsDirty(int32 Slot)
    {
        if (Slot >= TagsDirtyMask.Num())
            TagsDirtyMask.SetNum(Slot + 1, EAllowShrinking::No);
        if (!TagsDirtyMask[Slot]) { TagsDirtySlots.Add(Slot); TagsDirtyMask[Slot] = 1; }
    }

    FORCEINLINE void AddTag(FHktEntityPool& Pool, int32 Slot, const FGameplayTag& Tag)
    {
        Pool.TagContainers[Slot].AddTag(Tag);
        SetTagsDirty(Slot);
    }

    FORCEINLINE void RemoveTag(FHktEntityPool& Pool, int32 Slot, const FGameplayTag& Tag)
    {
        Pool.TagContainers[Slot].RemoveTag(Tag);
        SetTagsDirty(Slot);
    }

    FORCEINLINE void SetOwnerDirty(FHktEntityPool& Pool, int32 Slot, int64 Uid)
    {
        Pool.OwnerUids[Slot] = Uid;
        if (Slot >= OwnerDirtyMask.Num())
            OwnerDirtyMask.SetNum(Slot + 1, EAllowShrinking::No);
        if (!OwnerDirtyMask[Slot]) { OwnerDirtySlots.Add(Slot); OwnerDirtyMask[Slot] = 1; }
    }

    FORCEINLINE int64 GetPreFrameOwnerUid(int32 Slot) const
    {
        return (Slot < PreFrameOwnerUids.Num()) ? PreFrameOwnerUids[Slot] : 0;
    }

    template<typename F>
    void ForEachOwnerDirtyEntity(const FHktEntityPool& Pool, F&& Cb) const
    {
        for (int32 S : OwnerDirtySlots)
        {
            if (!Pool.SlotToEntity.IsValidIndex(S)) continue;
            FHktEntityId Id = Pool.SlotToEntity[S];
            if (Id != InvalidEntityId) Cb(Id, S);
        }
    }

    void ResetDirty(const FHktEntityPool& Pool);

    template<typename F>
    void ForEachDirtyEntity(const FHktEntityPool& Pool, F&& Cb) const
    {
        for (int32 S : DirtySlots)
        {
            if (!Pool.SlotToEntity.IsValidIndex(S)) continue;
            FHktEntityId Id = Pool.SlotToEntity[S];
            if (Id != InvalidEntityId) Cb(Id, S, DirtyMask[S]);
        }
    }

    template<typename F>
    void ForEachTagDirtyEntity(const FHktEntityPool& Pool, F&& Cb) const
    {
        for (int32 S : TagsDirtySlots)
        {
            if (!Pool.SlotToEntity.IsValidIndex(S)) continue;
            FHktEntityId Id = Pool.SlotToEntity[S];
            if (Id != InvalidEntityId) Cb(Id, S);
        }
    }

    FORCEINLINE int32 GetPreFrameValue(int32 Slot, uint16 PropId) const
    {
        return PreFrameData[Slot * FHktEntityPool::Stride + PropId];
    }

    FORCEINLINE const FGameplayTagContainer& GetPreFrameTags(int32 Slot) const
    {
        return PreFrameTagContainers[Slot];
    }
};

// ============================================================================
// FHktVMWorldStateProxy — VM dirty-aware 뮤테이션 API (CorePrivate 전용)
// ============================================================================

struct FHktVMWorldStateProxy
{
    FHktVMEntityPoolProxy PoolProxies[HktType::MaxTypes];

    void Initialize(const FHktWorldState& WS);
    void ResetDirtyIndices(const FHktWorldState& WS);

    void SetPropertyDirty(FHktWorldState& WS, FHktEntityId Entity, uint16 PropId, int32 Value);
    void SetOwnerUid(FHktWorldState& WS, FHktEntityId Entity, int64 Uid);
    void AddTag(FHktWorldState& WS, FHktEntityId Entity, const FGameplayTag& Tag);
    void RemoveTag(FHktWorldState& WS, FHktEntityId Entity, const FGameplayTag& Tag);

    FORCEINLINE void SetPosition(FHktWorldState& WS, FHktEntityId Entity, int32 X, int32 Y, int32 Z)
    {
        SetPropertyDirty(WS, Entity, PropertyId::PosX, X);
        SetPropertyDirty(WS, Entity, PropertyId::PosY, Y);
        SetPropertyDirty(WS, Entity, PropertyId::PosZ, Z);
    }

    FORCEINLINE void SetPosition(FHktWorldState& WS, FHktEntityId Entity, const FIntVector& Pos)
    {
        SetPosition(WS, Entity, Pos.X, Pos.Y, Pos.Z);
    }

    FORCEINLINE FHktVMEntityPoolProxy& GetProxy(FHktTypeId T) { return PoolProxies[T]; }
    FORCEINLINE const FHktVMEntityPoolProxy& GetProxy(FHktTypeId T) const { return PoolProxies[T]; }
};
