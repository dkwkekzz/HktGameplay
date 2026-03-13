// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktWorldState.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktVMWorldStateProxy — VM dirty-aware 뮤테이션 API (CorePrivate 전용)
//
// DirtyMask / DirtySlots — 프레임 내 변경 추적
// PreFrameData — 프레임 시작 스냅샷 (UndoDiff OldValue 조회)
// ============================================================================

struct FHktVMWorldStateProxy
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

    void Initialize(const FHktWorldState& WS);
    void ResetDirtyIndices(const FHktWorldState& WS);

    // --- Property Dirty ---
    FORCEINLINE void SetDirty(FHktWorldState& WS, int32 Slot, uint16 PropId, int32 V)
    {
        WS.Data[Slot * FHktWorldState::Stride + PropId] = V;
        if (Slot >= DirtyMask.Num())
        {
            DirtyMask.SetNum(Slot + 1, EAllowShrinking::No);
            TagsDirtyMask.SetNum(Slot + 1, EAllowShrinking::No);
        }
        if (DirtyMask[Slot] == 0) DirtySlots.Add(Slot);
        DirtyMask[Slot] |= (1ULL << PropId);
    }

    void SetPropertyDirty(FHktWorldState& WS, FHktEntityId Entity, uint16 PropId, int32 Value);

    // --- Tag Dirty ---
    FORCEINLINE void SetTagsDirty(int32 Slot)
    {
        if (Slot >= TagsDirtyMask.Num())
            TagsDirtyMask.SetNum(Slot + 1, EAllowShrinking::No);
        if (!TagsDirtyMask[Slot]) { TagsDirtySlots.Add(Slot); TagsDirtyMask[Slot] = 1; }
    }

    FORCEINLINE void AddTag(FHktWorldState& WS, FHktEntityId Entity, const FGameplayTag& Tag)
    {
        if (!WS.IsValidEntity(Entity)) return;
        int32 Slot = WS.GetSlot(Entity);
        WS.TagContainers[Slot].AddTag(Tag);
        SetTagsDirty(Slot);
    }

    FORCEINLINE void RemoveTag(FHktWorldState& WS, FHktEntityId Entity, const FGameplayTag& Tag)
    {
        if (!WS.IsValidEntity(Entity)) return;
        int32 Slot = WS.GetSlot(Entity);
        WS.TagContainers[Slot].RemoveTag(Tag);
        SetTagsDirty(Slot);
    }

    // --- Owner Dirty ---
    FORCEINLINE void SetOwnerDirty(FHktWorldState& WS, int32 Slot, int64 Uid)
    {
        WS.OwnerUids[Slot] = Uid;
        if (Slot >= OwnerDirtyMask.Num())
            OwnerDirtyMask.SetNum(Slot + 1, EAllowShrinking::No);
        if (!OwnerDirtyMask[Slot]) { OwnerDirtySlots.Add(Slot); OwnerDirtyMask[Slot] = 1; }
    }

    void SetOwnerUid(FHktWorldState& WS, FHktEntityId Entity, int64 Uid);

    // --- Position ---
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

    // --- PreFrame Access ---
    FORCEINLINE int32 GetPreFrameValue(int32 Slot, uint16 PropId) const
    {
        return PreFrameData[Slot * FHktWorldState::Stride + PropId];
    }

    FORCEINLINE const FGameplayTagContainer& GetPreFrameTags(int32 Slot) const
    {
        return PreFrameTagContainers[Slot];
    }

    FORCEINLINE int64 GetPreFrameOwnerUid(int32 Slot) const
    {
        return (Slot < PreFrameOwnerUids.Num()) ? PreFrameOwnerUids[Slot] : 0;
    }

    // --- Dirty Iteration ---
    template<typename F>
    void ForEachDirtyEntity(const FHktWorldState& WS, F&& Cb) const
    {
        for (int32 S : DirtySlots)
        {
            if (!WS.SlotToEntity.IsValidIndex(S)) continue;
            FHktEntityId Id = WS.SlotToEntity[S];
            if (Id != InvalidEntityId) Cb(Id, S, DirtyMask[S]);
        }
    }

    template<typename F>
    void ForEachTagDirtyEntity(const FHktWorldState& WS, F&& Cb) const
    {
        for (int32 S : TagsDirtySlots)
        {
            if (!WS.SlotToEntity.IsValidIndex(S)) continue;
            FHktEntityId Id = WS.SlotToEntity[S];
            if (Id != InvalidEntityId) Cb(Id, S);
        }
    }

    template<typename F>
    void ForEachOwnerDirtyEntity(const FHktWorldState& WS, F&& Cb) const
    {
        for (int32 S : OwnerDirtySlots)
        {
            if (!WS.SlotToEntity.IsValidIndex(S)) continue;
            FHktEntityId Id = WS.SlotToEntity[S];
            if (Id != InvalidEntityId) Cb(Id, S);
        }
    }
};
