// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreDefs.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktEntitySchema — 타입별 프로퍼티 메타데이터
//
//   PropertyIds[LocalIndex] = GlobalPropertyId
//   PropertyToLocal[GlobalPropertyId] = LocalIndex (stride 내 오프셋)
// ============================================================================

struct HKTCORE_API FHktEntitySchema
{
    FHktTypeId TypeId = HktType::None;
    TArray<uint16> PropertyIds;
    TArray<int8> PropertyToLocal;

    void AddProperty(uint16 PropId)
    {
        int8 LocalIdx = static_cast<int8>(PropertyIds.Num());
        PropertyIds.Add(PropId);
        if (PropId >= PropertyToLocal.Num())
        {
            int32 OldNum = PropertyToLocal.Num();
            PropertyToLocal.SetNum(PropId + 1);
            for (int32 i = OldNum; i < PropertyToLocal.Num(); ++i)
                PropertyToLocal[i] = -1;
        }
        PropertyToLocal[PropId] = LocalIdx;
    }

    bool HasProperty(uint16 PropId) const
    {
        return PropId < PropertyToLocal.Num() && PropertyToLocal[PropId] != -1;
    }

    FORCEINLINE int8 GetLocalIndex(uint16 PropId) const
    {
        return (PropId < PropertyToLocal.Num()) ? PropertyToLocal[PropId] : -1;
    }

    FORCEINLINE int32 GetStride() const { return PropertyIds.Num(); }
};

// ============================================================================
// FHktSchemaRegistry — 전역 스키마 등록소 (게임 초기화 시 1회)
// ============================================================================

struct HKTCORE_API FHktSchemaRegistry
{
    FHktEntitySchema Schemas[HktType::MaxTypes];

    void Initialize();

    FORCEINLINE const FHktEntitySchema& Get(FHktTypeId TypeId) const
    {
        check(TypeId < HktType::MaxTypes);
        return Schemas[TypeId];
    }
};

// ============================================================================
// FHktEntityPool — 타입별 Flat AOS 저장소
//
// 메모리 레이아웃 (예: Unit, Stride=20):
//   Data[] = [E0_P0 E0_P1 ... E0_P19 | E1_P0 E1_P1 ... E1_P19 | ...]
//            |<----- 80 bytes ------->|<----- 80 bytes ------->|
// ============================================================================

struct HKTCORE_API FHktEntityPool
{
    const FHktEntitySchema* Schema = nullptr;
    FHktTypeId TypeId = HktType::None;
    int32 Stride = 0;

    TArray<int32> Data;
    TArray<FHktEntityId> SlotToEntity;
    TArray<int32> FreeSlots;
    int32 ActiveCount = 0;

    TArray<uint32> DirtyMask;
    TArray<int32> DirtySlots;

    TArray<FGameplayTagContainer> TagContainers; // 슬롯 인덱스로 접근
    TArray<uint8>  TagsDirtyMask;               // 슬롯별 태그 변경 여부
    TArray<int32>  TagsDirtySlots;              // 변경된 슬롯 목록

    /** OwnedPlayerUid → Slot 역인덱스. 플레이어 소유 엔티티 O(1) 조회. */
    TMap<int64, TArray<int32>> OwnerToSlots;

    void Initialize(const FHktEntitySchema& InSchema, int32 ReserveCount);
    int32 AllocateSlot(FHktEntityId EntityId);
    void FreeSlot(int32 Slot);

    /** OwnedPlayerUid 변경 시 역인덱스 갱신 (WorldState::SetPropertyDirty에서 호출) */
    void TrackOwner(int32 Slot, int64 OldUid, int64 NewUid)
    {
        if (OldUid != 0)
        {
            if (TArray<int32>* OldSlots = OwnerToSlots.Find(OldUid))
                OldSlots->RemoveSwap(Slot);
        }
        if (NewUid != 0)
            OwnerToSlots.FindOrAdd(NewUid).Add(Slot);
    }

    /** 엔티티 제거 시 역인덱스에서 해당 슬롯 제거 (WorldState::RemoveEntity에서 호출) */
    void UntrackOwner(int32 Slot, int64 OwnerUid)
    {
        if (OwnerUid != 0)
            if (TArray<int32>* Slots = OwnerToSlots.Find(OwnerUid))
                Slots->RemoveSwap(Slot);
    }

    /** OwnerUid에 속한 엔티티만 순회 */
    template<typename F>
    void ForEachEntityByOwner(int64 OwnerUid, F&& Cb) const
    {
        const TArray<int32>* Slots = OwnerToSlots.Find(OwnerUid);
        if (!Slots) return;
        for (int32 S : *Slots)
            if (SlotToEntity.IsValidIndex(S) && SlotToEntity[S] != InvalidEntityId)
                Cb(SlotToEntity[S], S);
    }

    FORCEINLINE int32* EntityData(int32 Slot) { return Data.GetData() + Slot * Stride; }
    FORCEINLINE const int32* EntityData(int32 Slot) const { return Data.GetData() + Slot * Stride; }
    FORCEINLINE int32 Get(int32 Slot, int8 LP) const { return Data[Slot * Stride + LP]; }
    FORCEINLINE void Set(int32 Slot, int8 LP, int32 V) { Data[Slot * Stride + LP] = V; }

    FORCEINLINE void SetDirty(int32 Slot, int8 LP, int32 V)
    {
        Data[Slot * Stride + LP] = V;
        if (DirtyMask[Slot] == 0) DirtySlots.Add(Slot);
        DirtyMask[Slot] |= (1u << LP);
    }

    FORCEINLINE void SetTagsDirty(int32 Slot)
    {
        if (!TagsDirtyMask[Slot]) { TagsDirtySlots.Add(Slot); TagsDirtyMask[Slot] = 1; }
    }

    FORCEINLINE void AddTag(int32 Slot, const FGameplayTag& Tag)    { TagContainers[Slot].AddTag(Tag);    SetTagsDirty(Slot); }
    FORCEINLINE void RemoveTag(int32 Slot, const FGameplayTag& Tag) { TagContainers[Slot].RemoveTag(Tag); SetTagsDirty(Slot); }
    FORCEINLINE bool HasTag(int32 Slot, const FGameplayTag& Tag) const     { return TagContainers[Slot].HasTag(Tag); }
    FORCEINLINE bool HasTagExact(int32 Slot, const FGameplayTag& Tag) const { return TagContainers[Slot].HasTagExact(Tag); }
    FORCEINLINE const FGameplayTagContainer& GetTags(int32 Slot) const     { return TagContainers[Slot]; }

    void ResetDirty()
    {
        for (int32 S : DirtySlots)     DirtyMask[S] = 0;
        for (int32 S : TagsDirtySlots) TagsDirtyMask[S] = 0;
        DirtySlots.Reset(); TagsDirtySlots.Reset();
    }

    template<typename F> void ForEachTagDirtyEntity(F&& Cb) const
    {
        for (int32 S : TagsDirtySlots)
            if (SlotToEntity.IsValidIndex(S) && SlotToEntity[S] != InvalidEntityId)
                Cb(SlotToEntity[S], S);
    }

    template<typename F> void ForEachEntity(F&& Cb) const
    {
        for (int32 S = 0; S < SlotToEntity.Num(); ++S)
            if (SlotToEntity[S] != InvalidEntityId) Cb(SlotToEntity[S], S);
    }

    template<typename F> void ForEachDirtyEntity(F&& Cb) const
    {
        for (int32 S : DirtySlots)
        {
            FHktEntityId Id = SlotToEntity[S];
            if (Id != InvalidEntityId) Cb(Id, S, DirtyMask[S]);
        }
    }
};

// ============================================================================
// FHktWorldState — Archetype 기반 타입별 AOS
// ============================================================================

struct HKTCORE_API FHktWorldState
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    FHktEntityId NextEntityId = 0;

    struct FEntityLocation
    {
        FHktTypeId TypeId = HktType::None;
        int32 PoolSlot = -1;
    };

    TArray<FEntityLocation> EntityLocations;
    FHktEntityPool Pools[HktType::MaxTypes];
    const FHktSchemaRegistry* Registry = nullptr;
    TArray<FHktEvent> ActiveEvents;

    // --- Lifecycle ---
    void Initialize(const FHktSchemaRegistry& InRegistry);
    FHktEntityId AllocateEntity(FHktTypeId TypeId);
    void RemoveEntity(FHktEntityId Id);

    FORCEINLINE bool IsValidEntity(FHktEntityId Id) const
    {
        return Id >= 0 && Id < EntityLocations.Num()
            && EntityLocations[Id].TypeId != HktType::None;
    }

    FORCEINLINE FHktTypeId GetEntityType(FHktEntityId Id) const
    {
        return IsValidEntity(Id) ? EntityLocations[Id].TypeId : HktType::None;
    }

    // --- Property Access ---
    FORCEINLINE int32 GetProperty(FHktEntityId Entity, uint16 PropId) const
    {
        if (!IsValidEntity(Entity)) return 0;
        const FEntityLocation& L = EntityLocations[Entity];
        int8 LP = Registry->Get(L.TypeId).GetLocalIndex(PropId);
        return (LP != -1) ? Pools[L.TypeId].Get(L.PoolSlot, LP) : 0;
    }

    FORCEINLINE void SetPropertyDirty(FHktEntityId Entity, uint16 PropId, int32 Value)
    {
        if (!IsValidEntity(Entity)) return;
        const FEntityLocation& L = EntityLocations[Entity];
        int8 LP = Registry->Get(L.TypeId).GetLocalIndex(PropId);
        if (LP == -1) return;
        if (PropId == PropertyId::OwnedPlayerUid)
        {
            int64 OldUid = static_cast<int64>(Pools[L.TypeId].Get(L.PoolSlot, LP));
            Pools[L.TypeId].TrackOwner(L.PoolSlot, OldUid, static_cast<int64>(Value));
        }
        Pools[L.TypeId].SetDirty(L.PoolSlot, LP, Value);
    }

    // --- Tag Access ---
    const FGameplayTagContainer& GetTags(FHktEntityId Entity) const;
    void AddTag(FHktEntityId Entity, const FGameplayTag& Tag);
    void RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag);
    bool HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const;

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
        SetPropertyDirty(Entity, PropertyId::PosX, X);
        SetPropertyDirty(Entity, PropertyId::PosY, Y);
        SetPropertyDirty(Entity, PropertyId::PosZ, Z);
    }

    FORCEINLINE void SetPosition(FHktEntityId Entity, const FIntVector& Pos)
    {
        SetPosition(Entity, Pos.X, Pos.Y, Pos.Z);
    }

    // --- Pool Access ---
    FORCEINLINE FHktEntityPool& GetPool(FHktTypeId T) { return Pools[T]; }
    FORCEINLINE const FHktEntityPool& GetPool(FHktTypeId T) const { return Pools[T]; }

    // --- Iteration ---
    template<typename F> void ForEachEntity(F&& Cb) const
    {
        for (int32 T = 1; T < HktType::MaxTypes; ++T) Pools[T].ForEachEntity(Cb);
    }

    // --- State ---
    void ResetDirtyIndices();
    int32 GetEntityCount() const;

    // --- DTO ---
    FHktEntityState ExtractEntityState(FHktEntityId Id) const;
    FHktEntityId ImportEntityState(const FHktEntityState& InState);

    /** 전체 상태 복사 (클라이언트 동기화 등) */
    void CopyFrom(const FHktWorldState& Other);

    // --- Serialization ---
    friend HKTCORE_API FArchive& operator<<(FArchive& Ar, FHktWorldState& State);
};
