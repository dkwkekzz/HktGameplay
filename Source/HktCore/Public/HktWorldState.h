// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreDefs.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktEntityPool — 단일 Flat AOS 저장소
//
// Uniform Stride: Data[Slot * Stride + PropId]
// 타입 구분 없이 모든 엔티티를 하나의 배열에 저장.
// EntityType은 PropertyId::EntityType property로 조회.
// ============================================================================

struct HKTCORE_API FHktEntityPool
{
    static constexpr int32 Stride = PropertyId::MaxCount;

    TArray<int32> Data;
    TArray<FHktEntityId> SlotToEntity;
    TArray<int32> FreeSlots;
    int32 ActiveCount = 0;

    TArray<FGameplayTagContainer> TagContainers;
    TArray<int64> OwnerUids;

    void Initialize(int32 ReserveCount);
    int32 AllocateSlot(FHktEntityId EntityId);
    void FreeSlot(int32 Slot);

    FORCEINLINE int32* EntityData(int32 Slot) { return Data.GetData() + Slot * Stride; }
    FORCEINLINE const int32* EntityData(int32 Slot) const { return Data.GetData() + Slot * Stride; }
    FORCEINLINE int32 Get(int32 Slot, uint16 PropId) const { return Data[Slot * Stride + PropId]; }
    FORCEINLINE void Set(int32 Slot, uint16 PropId, int32 V) { Data[Slot * Stride + PropId] = V; }

    FORCEINLINE void AddTag(int32 Slot, const FGameplayTag& Tag)     { TagContainers[Slot].AddTag(Tag); }
    FORCEINLINE void RemoveTag(int32 Slot, const FGameplayTag& Tag)  { TagContainers[Slot].RemoveTag(Tag); }
    FORCEINLINE bool HasTag(int32 Slot, const FGameplayTag& Tag) const      { return TagContainers[Slot].HasTag(Tag); }
    FORCEINLINE bool HasTagExact(int32 Slot, const FGameplayTag& Tag) const { return TagContainers[Slot].HasTagExact(Tag); }
    FORCEINLINE const FGameplayTagContainer& GetTags(int32 Slot) const      { return TagContainers[Slot]; }

    template<typename F> void ForEachEntity(F&& Cb) const
    {
        for (int32 S = 0; S < SlotToEntity.Num(); ++S)
            if (SlotToEntity[S] != InvalidEntityId) Cb(SlotToEntity[S], S);
    }
};

// ============================================================================
// FHktWorldState — 단일 Pool 기반 (순수 게임 상태)
// ============================================================================

struct HKTCORE_API FHktWorldState
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    FHktEntityId NextEntityId = 0;

    TArray<int32> EntitySlots;   // EntityId → Pool Slot (-1 = invalid)
    FHktEntityPool Pool;
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
        return static_cast<FHktTypeId>(Pool.Get(EntitySlots[Id], PropertyId::EntityType));
    }

    // --- Property Access (직접 접근, 매핑 없음) ---
    FORCEINLINE int32 GetProperty(FHktEntityId Entity, uint16 PropId) const
    {
        if (!ensure(IsValidEntity(Entity))) return 0;
        return Pool.Get(EntitySlots[Entity], PropId);
    }

    FORCEINLINE void SetProperty(FHktEntityId Entity, uint16 PropId, int32 Value)
    {
        if (!ensure(IsValidEntity(Entity))) return;
        Pool.Set(EntitySlots[Entity], PropId, Value);
    }

    // --- Tag Access ---
    const FGameplayTagContainer& GetTags(FHktEntityId Entity) const;
    void AddTag(FHktEntityId Entity, const FGameplayTag& Tag);
    void RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag);
    bool HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const;

    // --- Owner Access ---
    FORCEINLINE int64 GetOwnerUid(FHktEntityId Entity) const
    {
        if (!ensure(IsValidEntity(Entity))) return 0;
        return Pool.OwnerUids[EntitySlots[Entity]];
    }

    FORCEINLINE void SetOwnerUid(FHktEntityId Entity, int64 Uid)
    {
        if (!ensure(IsValidEntity(Entity))) return;
        Pool.OwnerUids[EntitySlots[Entity]] = Uid;
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

    // --- Pool Access ---
    FORCEINLINE FHktEntityPool& GetPool() { return Pool; }
    FORCEINLINE const FHktEntityPool& GetPool() const { return Pool; }

    // --- Slot Access (내부 사용) ---
    FORCEINLINE int32 GetSlot(FHktEntityId Id) const { return EntitySlots[Id]; }

    // --- Iteration ---
    template<typename F> void ForEachEntity(F&& Cb) const
    {
        Pool.ForEachEntity(Forward<F>(Cb));
    }

    template<typename F> void ForEachEntityByType(FHktTypeId TypeId, F&& Cb) const
    {
        Pool.ForEachEntity([&](FHktEntityId Id, int32 Slot)
        {
            if (Pool.Get(Slot, PropertyId::EntityType) == TypeId)
                Cb(Id, Slot);
        });
    }

    template<typename F> void ForEachEntityByOwner(int64 OwnerUid, F&& Cb) const
    {
        if (Pool.ActiveCount == 0) return;
        Pool.ForEachEntity([&](FHktEntityId Id, int32 Slot)
        {
            if (Pool.OwnerUids[Slot] == OwnerUid)
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
};

template<>
struct TStructOpsTypeTraits<FHktWorldState> : public TStructOpsTypeTraitsBase2<FHktWorldState>
{
    enum { WithNetSerializer = true };
};
