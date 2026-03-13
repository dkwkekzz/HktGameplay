// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreDefs.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktPropertyPair — Warm/Overflow 페어 저장 단위
// ============================================================================

struct FHktPropertyPair
{
    static constexpr uint16 EmptyPropId = 0xFFFF;

    uint16 PropId = EmptyPropId;
    int32 Value = 0;

    FORCEINLINE bool IsEmpty() const { return PropId == EmptyPropId; }

    friend FArchive& operator<<(FArchive& Ar, FHktPropertyPair& P)
    {
        Ar << P.PropId << P.Value;
        return Ar;
    }
};

// ============================================================================
// FHktWorldState — 3-Tier Property Storage 기반 순수 게임 상태
//
// Hot:  HotData[Slot * HotStride + PropId]        — O(1) 직접 인덱싱
// Warm: WarmData[Slot * WarmCapacity + i]          — 선형 탐색 (고정 용량)
// Overflow: OverflowData[Slot]                     — 힙 TArray (Warm 초과 시)
// ============================================================================

struct HKTCORE_API FHktWorldState
{
    static constexpr int32 HotStride = PropertyId::HotMaxCount;
    static constexpr int32 WarmCapacity = 16;

    // --- Frame State ---
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    FHktEntityId NextEntityId = 0;

    // --- Entity Storage (3-Tier) ---
    TArray<int32> EntitySlots;                      // EntityId → Slot (-1 = invalid)
    TArray<int32> HotData;                          // Hot property storage
    TArray<FHktPropertyPair> WarmData;              // Warm property pairs
    TArray<TArray<FHktPropertyPair>> OverflowData;  // Heap overflow per slot
    TArray<FHktEntityId> SlotToEntity;              // Slot → EntityId reverse mapping
    TArray<int32> FreeSlots;                        // Free slot stack for reuse
    int32 ActiveCount = 0;

    TArray<FGameplayTagContainer> TagContainers;
    TArray<int64> OwnerUids;
    TArray<FHktEvent> ActiveEvents;

    // --- Lifecycle ---
    void Initialize();
    FHktEntityId AllocateEntity(FHktTypeId TypeId);
    void RemoveEntity(FHktEntityId Id);

    FORCEINLINE bool IsValidEntity(FHktEntityId Id) const
    {
        return Id >= 0 && Id < EntitySlots.Num() && EntitySlots[Id] >= 0;
    }

    FORCEINLINE FHktTypeId GetEntityType(FHktEntityId Id) const
    {
        if (!IsValidEntity(Id)) return HktType::None;
        return static_cast<FHktTypeId>(Get(EntitySlots[Id], PropertyId::EntityType));
    }

    // --- Slot-level Hot Data Access (내부/시스템용, Hot 전용) ---
    FORCEINLINE int32* HotEntityData(int32 Slot) { return HotData.GetData() + Slot * HotStride; }
    FORCEINLINE const int32* HotEntityData(int32 Slot) const { return HotData.GetData() + Slot * HotStride; }

    // --- 3-Tier Get/Set ---
    FORCEINLINE int32 Get(int32 Slot, uint16 PropId) const
    {
        if (PropId < HotStride)
        {
            return HotData[Slot * HotStride + PropId];
        }
        return GetCold(Slot, PropId);
    }

    FORCEINLINE void Set(int32 Slot, uint16 PropId, int32 V)
    {
        if (PropId < HotStride)
        {
            HotData[Slot * HotStride + PropId] = V;
            return;
        }
        SetCold(Slot, PropId, V);
    }

    // --- Property Access (EntityId 기반) ---
    FORCEINLINE int32 GetProperty(FHktEntityId Entity, uint16 PropId) const
    {
        if (!ensure(IsValidEntity(Entity))) return 0;
        return Get(EntitySlots[Entity], PropId);
    }

    FORCEINLINE void SetProperty(FHktEntityId Entity, uint16 PropId, int32 Value)
    {
        if (!ensure(IsValidEntity(Entity))) return;
        Set(EntitySlots[Entity], PropId, Value);
    }

    // --- Tag Access (EntityId 기반) ---
    const FGameplayTagContainer& GetTags(FHktEntityId Entity) const;
    void AddTag(FHktEntityId Entity, const FGameplayTag& Tag);
    void RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag);
    bool HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const;

    // --- Tag Access (Slot 기반, 내부용) ---
    FORCEINLINE const FGameplayTagContainer& GetTagsBySlot(int32 Slot) const { return TagContainers[Slot]; }

    // --- Owner Access ---
    FORCEINLINE int64 GetOwnerUid(FHktEntityId Entity) const
    {
        if (!ensure(IsValidEntity(Entity))) return 0;
        return OwnerUids[EntitySlots[Entity]];
    }

    FORCEINLINE void SetOwnerUid(FHktEntityId Entity, int64 Uid)
    {
        if (!ensure(IsValidEntity(Entity))) return;
        OwnerUids[EntitySlots[Entity]] = Uid;
    }

    // --- Position shortcuts ---
    FORCEINLINE FIntVector GetPosition(FHktEntityId Entity) const
    {
        return FIntVector(
            GetProperty(Entity, PropertyId::PosX),
            GetProperty(Entity, PropertyId::PosY),
            GetProperty(Entity, PropertyId::PosZ));
    }

    FORCEINLINE void SetPosition(FHktEntityId Entity, int32 X, int32 Y, int32 Z)
    {
        SetProperty(Entity, PropertyId::PosX, X);
        SetProperty(Entity, PropertyId::PosY, Y);
        SetProperty(Entity, PropertyId::PosZ, Z);
    }

    FORCEINLINE void SetPosition(FHktEntityId Entity, const FIntVector& Pos)
    {
        SetPosition(Entity, Pos.X, Pos.Y, Pos.Z);
    }

    // --- Slot Access (내부 사용) ---
    FORCEINLINE int32 GetSlot(FHktEntityId Id) const { return EntitySlots[Id]; }

    // --- Iteration ---
    template<typename F> void ForEachEntity(F&& Cb) const
    {
        for (int32 S = 0; S < SlotToEntity.Num(); ++S)
            if (SlotToEntity[S] != InvalidEntityId) Cb(SlotToEntity[S], S);
    }

    template<typename F> void ForEachEntityByType(FHktTypeId TypeId, F&& Cb) const
    {
        ForEachEntity([&](FHktEntityId Id, int32 Slot)
        {
            if (Get(Slot, PropertyId::EntityType) == TypeId)
                Cb(Id, Slot);
        });
    }

    template<typename F> void ForEachEntityByOwner(int64 OwnerUid, F&& Cb) const
    {
        if (ActiveCount == 0) return;
        ForEachEntity([&](FHktEntityId Id, int32 Slot)
        {
            if (OwnerUids[Slot] == OwnerUid)
                Cb(Id, Slot);
        });
    }

    // --- State ---
    int32 GetEntityCount() const;

    // --- DTO ---
    FHktEntityState ExtractEntityState(FHktEntityId Id) const;
    FHktEntityId ImportEntityState(const FHktEntityState& InState);
    void ImportEntityStateWithId(const FHktEntityState& InState);

    /** Diff 역적용 — 프레임 변경 되돌리기 */
    void UndoDiff(const FHktSimulationDiff& Diff);

    /** 전체 상태 복사 */
    void CopyFrom(const FHktWorldState& Other);

    // --- Serialization ---
    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

private:
    int32 AllocateSlot(FHktEntityId EntityId);
    void FreeSlot(int32 Slot);
    void ClearSlotWarm(int32 Slot);

    // Cold (Warm+Overflow) 접근 헬퍼
    int32 GetCold(int32 Slot, uint16 PropId) const;
    void SetCold(int32 Slot, uint16 PropId, int32 V);
};

template<>
struct TStructOpsTypeTraits<FHktWorldState> : public TStructOpsTypeTraitsBase2<FHktWorldState>
{
    enum { WithNetSerializer = true };
};
