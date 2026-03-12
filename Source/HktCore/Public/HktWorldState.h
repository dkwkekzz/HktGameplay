// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreDefs.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"

// ============================================================================
// FHktPropertyMask — 프로퍼티 유효성 비트마스크
//
// PropertyId::MaxCount <= 64 이면 uint64 1개, 그 이상이면 배열로 확장.
// ============================================================================

struct FHktPropertyMask
{
    static constexpr int32 NumWords = (PropertyId::MaxCount + 63) / 64;
    uint64 Bits[NumWords] = {};

    FORCEINLINE void Set(uint16 PropId)       { Bits[PropId >> 6] |= (1ULL << (PropId & 63)); }
    FORCEINLINE bool Test(uint16 PropId) const { return (Bits[PropId >> 6] & (1ULL << (PropId & 63))) != 0; }
    FORCEINLINE bool IsZero() const
    {
        for (int32 i = 0; i < NumWords; ++i) if (Bits[i] != 0) return false;
        return true;
    }
};

// ============================================================================
// FHktEntitySchema — 타입별 프로퍼티 메타데이터 (디버그 검증용)
//
// Uniform Stride: PropertyId가 곧 배열 오프셋. 매핑 불필요.
// ValidMask는 디버그 빌드에서 잘못된 property 접근 감지용.
// ============================================================================

struct HKTCORE_API FHktEntitySchema
{
    FHktTypeId TypeId = HktType::None;
    FHktPropertyMask ValidMask;

    void MarkValid(uint16 PropId)
    {
        checkf(PropId < PropertyId::MaxCount, TEXT("PropId %u out of range"), PropId);
        ValidMask.Set(PropId);
    }

    FORCEINLINE bool HasProperty(uint16 PropId) const
    {
        return PropId < PropertyId::MaxCount && ValidMask.Test(PropId);
    }

    static constexpr int32 GetStride() { return PropertyId::MaxCount; }
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
// Uniform Stride: 모든 타입이 PropertyId::MaxCount 고정.
// Data[Slot * Stride + PropId] — PropertyId가 곧 오프셋.
// ============================================================================

struct HKTCORE_API FHktEntityPool
{
    FHktTypeId TypeId = HktType::None;
    static constexpr int32 Stride = PropertyId::MaxCount;

    TArray<int32> Data;
    TArray<FHktEntityId> SlotToEntity;
    TArray<int32> FreeSlots;
    int32 ActiveCount = 0;

    TArray<FGameplayTagContainer> TagContainers;
    TArray<int64> OwnerUids;  // SlotToEntity/TagContainers와 병렬

    void Initialize(FHktTypeId InTypeId, int32 ReserveCount);
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

    // --- Property Access (직접 접근, 매핑 없음) ---
    FORCEINLINE int32 GetProperty(FHktEntityId Entity, uint16 PropId) const
    {
        if (!ensure(IsValidEntity(Entity))) return 0;
        const FEntityLocation& L = EntityLocations[Entity];
        checkSlow(FHktSchemaRegistry::Get().Get(L.TypeId).HasProperty(PropId));
        return Pools[L.TypeId].Get(L.PoolSlot, PropId);
    }

    FORCEINLINE void SetProperty(FHktEntityId Entity, uint16 PropId, int32 Value)
    {
        if (!ensure(IsValidEntity(Entity))) return;
        const FEntityLocation& L = EntityLocations[Entity];
        checkSlow(FHktSchemaRegistry::Get().Get(L.TypeId).HasProperty(PropId));
        Pools[L.TypeId].Set(L.PoolSlot, PropId, Value);
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
