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
// FHktSchemaRegistry — 전역 스키마 등록소
//
// 싱글톤 패턴: FHktSchemaRegistry::Get() 으로 접근.
// 게임 초기화 시 최초 접근으로 자동 초기화됨.
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

    /** 모듈 전역 싱글톤 */
    static FHktSchemaRegistry& Get();
};

// ============================================================================
// FHktEntityPool — 타입별 Flat AOS 저장소 (순수 게임 상태)
//
// 메모리 레이아웃 (예: Unit, Stride=20):
//   Data[] = [E0_P0 E0_P1 ... E0_P19 | E1_P0 E1_P1 ... E1_P19 | ...]
//
// VM 처리 중간물(DirtyMask, PreFrameData 등)은 HktVMWorldStateProxy에 격리.
// ============================================================================

struct HKTCORE_API FHktEntityPool
{
    FHktTypeId TypeId = HktType::None;
    int32 Stride = 0;

    TArray<int32> Data;
    TArray<FHktEntityId> SlotToEntity;
    TArray<int32> FreeSlots;
    int32 ActiveCount = 0;

    TArray<FGameplayTagContainer> TagContainers;
    TArray<int64> OwnerUids;  // SlotToEntity/TagContainers와 병렬

    void Initialize(const FHktEntitySchema& InSchema, int32 ReserveCount);
    int32 AllocateSlot(FHktEntityId EntityId);
    void FreeSlot(int32 Slot);

    FORCEINLINE int32* EntityData(int32 Slot) { return Data.GetData() + Slot * Stride; }
    FORCEINLINE const int32* EntityData(int32 Slot) const { return Data.GetData() + Slot * Stride; }
    FORCEINLINE int32 Get(int32 Slot, int8 LP) const { return Data[Slot * Stride + LP]; }
    FORCEINLINE void Set(int32 Slot, int8 LP, int32 V) { Data[Slot * Stride + LP] = V; }

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
// FHktWorldState — Archetype 기반 타입별 AOS (순수 게임 상태)
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
    TArray<FHktEvent> ActiveEvents;

    // --- Lifecycle ---
    void Initialize();
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
        if (!ensure(IsValidEntity(Entity))) return 0;
        const FEntityLocation& L = EntityLocations[Entity];
        int8 LP = FHktSchemaRegistry::Get().Get(L.TypeId).GetLocalIndex(PropId);
        if (ensure(LP != -1) == false) return 0;
        return Pools[L.TypeId].Get(L.PoolSlot, LP);
    }

    FORCEINLINE void SetProperty(FHktEntityId Entity, uint16 PropId, int32 Value)
    {
        if (!ensure(IsValidEntity(Entity))) return;
        const FEntityLocation& L = EntityLocations[Entity];
        int8 LP = FHktSchemaRegistry::Get().Get(L.TypeId).GetLocalIndex(PropId);
        if (ensure(LP != -1) == false) return;
        Pools[L.TypeId].Set(L.PoolSlot, LP, Value);
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
        const FEntityLocation& L = EntityLocations[Entity];
        return Pools[L.TypeId].OwnerUids[L.PoolSlot];
    }

    FORCEINLINE void SetOwnerUid(FHktEntityId Entity, int64 Uid)
    {
        if (!ensure(IsValidEntity(Entity))) return;
        const FEntityLocation& L = EntityLocations[Entity];
        Pools[L.TypeId].OwnerUids[L.PoolSlot] = Uid;
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
    FORCEINLINE FHktEntityPool& GetPool(FHktTypeId T) { return Pools[T]; }
    FORCEINLINE const FHktEntityPool& GetPool(FHktTypeId T) const { return Pools[T]; }

    // --- Iteration ---
    template<typename F> void ForEachEntity(F&& Cb) const
    {
        for (int32 T = 1; T < HktType::MaxTypes; ++T) Pools[T].ForEachEntity(Cb);
    }

    /** OwnerUid에 속한 모든 엔티티 순회 (O(N) 선형 스캔, N=풀 슬롯 수) */
    template<typename F> void ForEachEntityByOwner(int64 OwnerUid, F&& Cb) const
    {
        for (int32 T = 1; T < HktType::MaxTypes; ++T)
        {
            const FHktEntityPool& Pool = Pools[T];
            if (Pool.ActiveCount == 0) continue;
            Pool.ForEachEntity([&](FHktEntityId Id, int32 Slot)
            {
                if (Pool.OwnerUids[Slot] == OwnerUid)
                    Cb(Id, Slot);
            });
        }
    }

    // --- State ---
    int32 GetEntityCount() const;

    // --- DTO ---
    FHktEntityState ExtractEntityState(FHktEntityId Id) const;
    FHktEntityId ImportEntityState(const FHktEntityState& InState);

    /** 지정된 EntityId로 엔티티 복원 (UndoDiff에서 제거된 엔티티 복원용) */
    void ImportEntityStateWithId(const FHktEntityState& InState);

    /** Diff 역적용 — 프레임 변경 되돌리기 (클라이언트 예측 롤백용) */
    void UndoDiff(const FHktSimulationDiff& Diff);

    /** 전체 상태 복사 */
    void CopyFrom(const FHktWorldState& Other);

    // --- Serialization ---

    /** 네트워크 직렬화 (활성 엔티티만 송수신) */
    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FHktWorldState> : public TStructOpsTypeTraitsBase2<FHktWorldState>
{
    enum { WithNetSerializer = true };
};
