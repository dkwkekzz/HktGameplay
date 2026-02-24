// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreMinimal.h"
#include "HktEvents.h"

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

    void Initialize(const FHktEntitySchema& InSchema, int32 ReserveCount);
    int32 AllocateSlot(FHktEntityId EntityId);
    void FreeSlot(int32 Slot);

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

    void ResetDirty()
    {
        for (int32 S : DirtySlots) DirtyMask[S] = 0;
        DirtySlots.Reset();
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
        if (LP != -1) Pools[L.TypeId].SetDirty(L.PoolSlot, LP, Value);
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
