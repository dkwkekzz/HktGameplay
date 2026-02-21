// Copyright Hkt Studios, Inc. All Rights Reserved.
// [Flat SOA Refactor] - 분리형 SOA → 단일 연속 버퍼 Flat SOA

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

// ============================================================================
// Basic Types & IDs
// ============================================================================

using FHktEntityId = int32;
constexpr FHktEntityId InvalidEntityId = -1;

// ============================================================================
// Simulation Limits — 고정 버퍼 크기 상수
// ============================================================================

namespace HktLimits
{
    constexpr int32 MaxEntities = 1024;
    constexpr int32 MaxProperties = 64;       // PropertyId 최대값 52 + 여유
    constexpr int32 MaxVMs = 256;
    constexpr int32 MaxActiveEvents = 256;
    constexpr int32 MaxPendingEvents = 512;
    constexpr int32 MaxPhysicsEvents = 256;
    constexpr int32 MaxOverlayEntries = 1024;
    constexpr int32 MaxSpatialResults = 128;
    constexpr int32 MaxDirtyPerColumn = 256;
    constexpr int32 MaxPendingWritesPerVM = 64;
    constexpr int32 MaxLocalCachePerVM = 64;
}

// =========================================================================
// [인라인 페이로드 시스템]
// =========================================================================
struct FHktInlinePayload
{
    static constexpr int32 Capacity = 48;
    uint8 Data[Capacity] = { 0 };
    uint8 Size = 0;

    FHktInlinePayload() = default;

    template <typename... Args>
    void Set(Args... InArgs)
    {
        Size = 0;
        (Write(InArgs), ...);
    }

    template <typename T>
    void Write(const T& Value)
    {
        static_assert(TIsBitwiseConstructible<T>::Value, "Only bitwise-copyable types are supported.");
        if (Size + sizeof(T) <= Capacity)
        {
            FMemory::Memcpy(Data + Size, &Value, sizeof(T));
            Size += sizeof(T);
        }
    }

    template <typename T>
    T Get(int32 Offset) const
    {
        T Result;
        if (Offset + sizeof(T) <= Size) FMemory::Memcpy(&Result, Data + Offset, sizeof(T));
        else FMemory::Memset(&Result, 0, sizeof(T));
        return Result;
    }

    bool operator==(const FHktInlinePayload& Other) const
    {
        return Size == Other.Size && FMemory::Memcmp(Data, Other.Data, Size) == 0;
    }
    bool operator!=(const FHktInlinePayload& Other) const { return !(*this == Other); }

    friend FArchive& operator<<(FArchive& Ar, FHktInlinePayload& P)
    {
        Ar << P.Size;
        if (Ar.IsLoading()) FMemory::Memset(P.Data, 0, Capacity);
        Ar.Serialize(P.Data, P.Size);
        return Ar;
    }
};

// ============================================================================
// Events
// ============================================================================

struct HKTCORE_API FHktEvent
{
    int32 EventId = 0;
    FGameplayTag EventTag;
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    FVector Location = FVector::ZeroVector;
    int32 Param0 = 0;
    int32 Param1 = 0;
    FHktInlinePayload Payload;

    bool IsValid() const { return EventTag.IsValid(); }

    FString ToString() const
    {
        return FString::Printf(TEXT("EventId=%d Tag=%s Src=%d Tgt=%d"), EventId, *EventTag.ToString(), SourceEntity, TargetEntity);
    }

    friend FArchive& operator<<(FArchive& Ar, FHktEvent& Event)
    {
        Ar << Event.EventId;
        Ar << Event.EventTag;
        Ar << Event.SourceEntity;
        Ar << Event.TargetEntity;
        Ar << Event.Location;
        Ar << Event.Param0;
        Ar << Event.Param1;
        Ar << Event.Payload;
        return Ar;
    }
};

struct HKTCORE_API FHktPhysicsEvent
{
    FHktEntityId EntityA = InvalidEntityId;
    FHktEntityId EntityB = InvalidEntityId;
    FVector ContactPoint = FVector::ZeroVector;
};

// ============================================================================
// Entity State (DTO — 네트워크/DB 직렬화 전용)
// FHktSimulationEvent.RestoredEntityStates에서 참조하므로 먼저 정의
// ============================================================================

struct HKTCORE_API FHktEntityState
{
    FHktEntityId EntityId = InvalidEntityId;
    FVector Position = FVector::ZeroVector;
    TArray<int32> TagIndices;
    TArray<int32> Properties;

    int32 GetProperty(uint16 PropertyId) const
    {
        return (PropertyId < static_cast<uint16>(Properties.Num())) ? Properties[PropertyId] : 0;
    }

    void SetProperty(uint16 PropertyId, int32 Value)
    {
        if (PropertyId >= static_cast<uint16>(Properties.Num()))
        {
            Properties.SetNumZeroed(PropertyId + 1);
        }
        Properties[PropertyId] = Value;
    }

    friend FArchive& operator<<(FArchive& Ar, FHktEntityState& State)
    {
        Ar << State.EntityId;
        Ar << State.Position;
        Ar << State.TagIndices;
        Ar << State.Properties;
        return Ar;
    }
};

// ============================================================================
// Simulation Event
// ============================================================================

struct HKTCORE_API FHktSimulationEvent
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    float DeltaSeconds = 0.f;
    TArray<int64> RemovedOwnerIds;
    TArray<FHktEvent> Events;

    // [EntityStates 복원] 재접속/그룹 이동 시 DB 또는 소스 그룹에서 추출된 엔터티 상태
    // ProcessBatch의 Phase 1-0에서 WorldState에 주입됨
    TArray<FHktEntityState> RestoredEntityStates;

    void Reset()
    {
        FrameNumber = 0;
        RandomSeed = 0;
        DeltaSeconds = 0.f;
        RemovedOwnerIds.Reset();
        Events.Reset();
        RestoredEntityStates.Reset();
    }

    friend FArchive& operator<<(FArchive& Ar, FHktSimulationEvent& SimEvent)
    {
        Ar << SimEvent.FrameNumber;
        Ar << SimEvent.RandomSeed;
        Ar << SimEvent.DeltaSeconds;
        Ar << SimEvent.RemovedOwnerIds;
        Ar << SimEvent.Events;
        Ar << SimEvent.RestoredEntityStates;
        return Ar;
    }
};

// ============================================================================
// [Flat SOA] DirtyTracker — PropertyId별 변경 SlotIndex 추적
// ============================================================================

struct HKTCORE_API FHktDirtyTracker
{
    /** DirtyIndices[PropertyId] = 이번 프레임에 변경된 SlotIndex 목록 */
    TArray<TArray<int32>> DirtyIndices;

    void Initialize(int32 NumProperties)
    {
        DirtyIndices.SetNum(NumProperties);
        for (auto& Arr : DirtyIndices)
        {
            Arr.Reserve(HktLimits::MaxDirtyPerColumn);
        }
    }

    void MarkDirty(int32 PropertyId, int32 SlotIndex)
    {
        if (DirtyIndices.IsValidIndex(PropertyId))
        {
            DirtyIndices[PropertyId].Add(SlotIndex);
        }
    }

    const TArray<int32>& GetDirtySlots(int32 PropertyId) const
    {
        static const TArray<int32> Empty;
        return DirtyIndices.IsValidIndex(PropertyId) ? DirtyIndices[PropertyId] : Empty;
    }

    void ResetAll()
    {
        for (auto& Arr : DirtyIndices)
        {
            Arr.Reset(); // 용량 유지, 카운트만 0
        }
    }

    void EnsurePropertyId(int32 PropertyId)
    {
        if (PropertyId >= DirtyIndices.Num())
        {
            int32 OldNum = DirtyIndices.Num();
            DirtyIndices.SetNum(PropertyId + 1);
            for (int32 i = OldNum; i < DirtyIndices.Num(); ++i)
            {
                DirtyIndices[i].Reserve(HktLimits::MaxDirtyPerColumn);
            }
        }
    }
};

// ============================================================================
// [Flat SOA] FHktWorldState — 단일 연속 버퍼 기반
// ============================================================================
//
// 메모리 레이아웃:
//   FlatData[PropertyId * MaxSlots + SlotIndex] = int32 값
//
// 이전 분리형 SOA:  64개의 독립 TArray<int32> (64개 heap 블록)
// 새 Flat SOA:      1개의 TArray<int32> (1개 heap 블록)
//
// 이점:
//   - CopyFrom: memcpy 1회 (기존 64회)
//   - VM 랜덤 접근: 같은 메모리 영역 내 오프셋 점프 (TLB 미스 감소)
//   - 시스템 벌크 순회: 컬럼 내부 연속성 동일 (GetColumnPtr)
//
// ============================================================================

struct HKTCORE_API FHktWorldState
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;

    /** 다음 엔티티 할당 시 사용할 ID */
    FHktEntityId NextEntityId = 0;

    // --- SOA Entity Index Mapping ---
    TArray<int32> EntityToIndex;         // EntityId -> SlotIndex (-1 = invalid)
    TArray<FHktEntityId> IndexToEntity;  // SlotIndex -> EntityId
    TArray<int32> FreeIndices;           // 재사용 가능 슬롯

    // --- [Flat SOA] 단일 연속 데이터 버퍼 ---
    // Layout: FlatData[PropertyId * MaxSlots + SlotIndex]
    TArray<int32> FlatData;
    int32 NumProperties = 0;    // 현재 활성 Property 수
    int32 MaxSlots = 0;         // 현재 할당된 슬롯 수 (== IndexToEntity의 예약 크기)

    // --- [Flat SOA] 변경 추적 ---
    FHktDirtyTracker DirtyTracker;

    // --- Tag 데이터 (Property 시스템 밖이므로 별도 관리) ---
    TArray<TArray<int32>> TagColumn;  // SlotIndex -> TagIndices

    // --- Active Events ---
    TArray<FHktEvent> ActiveEvents;

    // ========================================================================
    // Flat SOA 접근자 (인라인 — 핫 경로)
    // ========================================================================

    /** 단일 값 읽기 (VM Store fallback, 외부 API) */
    FORCEINLINE int32 GetProperty(FHktEntityId Entity, uint16 PropertyId) const
    {
        if (!IsValidEntity(Entity)) return 0;
        const int32 SlotIndex = EntityToIndex[Entity];
        const int32 PropId = static_cast<int32>(PropertyId);
        if (PropId >= NumProperties) return 0;
        return FlatData[PropId * MaxSlots + SlotIndex];
    }

    /** 단일 값 쓰기 (시스템에서 직접 사용) */
    FORCEINLINE void SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value)
    {
        if (!IsValidEntity(Entity)) return;
        const int32 SlotIndex = EntityToIndex[Entity];
        const int32 PropId = static_cast<int32>(PropertyId);
        EnsurePropertyCapacity(PropId);
        FlatData[PropId * MaxSlots + SlotIndex] = Value;
    }

    /** 값 쓰기 + Dirty 마킹 (ApplyStoreSystem에서 사용) */
    FORCEINLINE void SetPropertyDirty(int32 SlotIndex, int32 PropertyId, int32 Value)
    {
        EnsurePropertyCapacity(PropertyId);
        FlatData[PropertyId * MaxSlots + SlotIndex] = Value;
        DirtyTracker.MarkDirty(PropertyId, SlotIndex);
    }

    /** 컬럼 시작 포인터 (시스템 벌크 순회용 — 루프 밖에서 캐싱) */
    FORCEINLINE const int32* GetColumnPtr(int32 PropertyId) const
    {
        if (PropertyId >= NumProperties || MaxSlots == 0) return nullptr;
        return FlatData.GetData() + (PropertyId * MaxSlots);
    }

    FORCEINLINE int32* GetMutableColumnPtr(int32 PropertyId)
    {
        EnsurePropertyCapacity(PropertyId);
        return FlatData.GetData() + (PropertyId * MaxSlots);
    }

    /** 특정 SlotIndex에서 직접 읽기 (컬럼 포인터 캐싱 후 사용) */
    FORCEINLINE int32 GetFast(int32 PropertyId, int32 SlotIndex) const
    {
        return FlatData[PropertyId * MaxSlots + SlotIndex];
    }

    // ========================================================================
    // 하위 호환 — 기존 GetColumn 인터페이스 (읽기 전용 래퍼)
    // ========================================================================

    /** [하위 호환] FHktDataColumn-like 읽기 래퍼 */
    struct FColumnView
    {
        const int32* Data = nullptr;
        int32 Count = 0;

        FORCEINLINE int32 GetInt(int32 SlotIndex) const
        {
            return (Data && SlotIndex >= 0 && SlotIndex < Count) ? Data[SlotIndex] : 0;
        }
    };

    /** [하위 호환] 시스템이 기존 패턴으로 사용 가능 */
    FORCEINLINE FColumnView GetColumn(int32 PropertyId) const
    {
        if (PropertyId < 0 || PropertyId >= NumProperties || MaxSlots == 0)
            return { nullptr, 0 };
        return { FlatData.GetData() + (PropertyId * MaxSlots), MaxSlots };
    }

    /** [하위 호환] DirtyIndices 접근 */
    const TArray<int32>& GetDirtySlots(int32 PropertyId) const
    {
        return DirtyTracker.GetDirtySlots(PropertyId);
    }

    // ========================================================================
    // Core Operations
    // ========================================================================

    FHktEntityId AllocateEntity();
    void RemoveEntity(FHktEntityId Id);

    FORCEINLINE bool IsValidEntity(FHktEntityId Id) const
    {
        return Id >= 0 && Id < EntityToIndex.Num() && EntityToIndex[Id] != -1;
    }

    FORCEINLINE int32 GetIndex(FHktEntityId Id) const
    {
        return IsValidEntity(Id) ? EntityToIndex[Id] : -1;
    }

    int32 GetEntityCount() const
    {
        return IndexToEntity.Num() - FreeIndices.Num();
    }

    /** 고정 버퍼 사전 할당 (생성 시 1회 호출) */
    void Initialize();

    /** 모든 DirtyIndices를 Reset (프레임 시작 시 호출) */
    void ResetDirtyIndices()
    {
        DirtyTracker.ResetAll();
    }

    // --- Iteration ---
    template<typename Func>
    void ForEachEntity(Func&& Callback) const
    {
        for (int32 SlotIndex = 0; SlotIndex < IndexToEntity.Num(); ++SlotIndex)
        {
            FHktEntityId Id = IndexToEntity[SlotIndex];
            if (Id != InvalidEntityId)
            {
                Callback(Id, SlotIndex);
            }
        }
    }

    // --- DTO 변환 ---
    FHktEntityState ExtractEntityState(FHktEntityId Id) const;

    // --- [EntityStates 복원] DB/그룹 이동에서 로드된 EntityStates를 Flat SOA에 주입 ---
    void RestoreEntities(const TArray<FHktEntityState>& InEntityStates);

    // --- Snapshot/Rollback ---
    void CopyFrom(const FHktWorldState& Other);

    // --- 직렬화 ---
    friend HKTCORE_API FArchive& operator<<(FArchive& Ar, FHktWorldState& WorldState);

private:
    /** PropertyId가 현재 용량을 넘으면 FlatData 확장 */
    void EnsurePropertyCapacity(int32 PropertyId);

    /** 슬롯 수 확장 (엔터티 추가 시) */
    void GrowSlots(int32 NewMaxSlots);
};

// ============================================================================
// World View (Zero-Copy Interface)
// ============================================================================

struct HKTCORE_API FHktOverlayEntry
{
    int32 PropertyId;
    FHktEntityId EntityId;
    int32 Value;

    bool operator<(const FHktOverlayEntry& Other) const
    {
        if (PropertyId != Other.PropertyId) return PropertyId < Other.PropertyId;
        return EntityId < Other.EntityId;
    }
};

struct HKTCORE_API FHktWorldView
{
    const FHktWorldState* WorldState = nullptr;
    TArray<FHktOverlayEntry> IntOverlays;

    int32 GetValue(FHktEntityId Entity, int32 PropertyId) const
    {
        // 1. Overlay 이진 탐색
        int32 Low = 0;
        int32 High = IntOverlays.Num() - 1;
        while (Low <= High)
        {
            int32 Mid = (Low + High) / 2;
            const FHktOverlayEntry& Entry = IntOverlays[Mid];
            if (Entry.PropertyId < PropertyId) Low = Mid + 1;
            else if (Entry.PropertyId > PropertyId) High = Mid - 1;
            else
            {
                if (Entry.EntityId < Entity) Low = Mid + 1;
                else if (Entry.EntityId > Entity) High = Mid - 1;
                else return Entry.Value;
            }
        }

        // 2. WorldState Flat SOA
        if (WorldState)
        {
            return WorldState->GetProperty(Entity, static_cast<uint16>(PropertyId));
        }
        return 0;
    }

    template<typename Func>
    void ForEachEntity(int32 PropertyId, Func&& Callback) const
    {
        if (WorldState)
        {
            for (const FHktEntityId& EntityId : WorldState->IndexToEntity)
            {
                Callback(EntityId);
            }
        }
    }

    template<typename Func>
    void ForEachDirtyEntity(int32 PropertyId, Func&& Callback) const
    {
        if (!WorldState) return;

        // 1. 커밋된 변경
        const TArray<int32>& DirtySlots = WorldState->GetDirtySlots(PropertyId);
        const int32* ColPtr = WorldState->GetColumnPtr(PropertyId);
        if (ColPtr)
        {
            for (int32 Idx : DirtySlots)
            {
                FHktEntityId EntityId = WorldState->IndexToEntity[Idx];
                if (EntityId != InvalidEntityId)
                {
                    Callback(EntityId, ColPtr[Idx]);
                }
            }
        }

        // 2. 미커밋 Overlay
        int32 Low = 0, High = IntOverlays.Num() - 1;
        int32 Start = IntOverlays.Num();
        while (Low <= High)
        {
            int32 Mid = (Low + High) / 2;
            if (IntOverlays[Mid].PropertyId < PropertyId) Low = Mid + 1;
            else
            {
                if (IntOverlays[Mid].PropertyId == PropertyId) Start = Mid;
                High = Mid - 1;
            }
        }
        for (int32 i = Start; i < IntOverlays.Num() && IntOverlays[i].PropertyId == PropertyId; ++i)
        {
            Callback(IntOverlays[i].EntityId, IntOverlays[i].Value);
        }
    }

    template<typename Func>
    void ForEachDirtyEntry(Func&& Callback) const
    {
        if (!WorldState) return;

        // 1. 커밋된 변경
        for (int32 PropId = 0; PropId < WorldState->NumProperties; ++PropId)
        {
            const TArray<int32>& DirtySlots = WorldState->GetDirtySlots(PropId);
            const int32* ColPtr = WorldState->GetColumnPtr(PropId);
            if (!ColPtr) continue;
            for (int32 Idx : DirtySlots)
            {
                FHktEntityId EntityId = WorldState->IndexToEntity[Idx];
                if (EntityId != InvalidEntityId)
                {
                    Callback(PropId, EntityId, ColPtr[Idx]);
                }
            }
        }

        // 2. 미커밋 Overlay
        for (const FHktOverlayEntry& Entry : IntOverlays)
        {
            Callback(Entry.PropertyId, Entry.EntityId, Entry.Value);
        }
    }

    const TArray<FHktEntityId>& GetAllEntities() const
    {
        return WorldState ? WorldState->IndexToEntity : DummyEntities;
    }

private:
    static inline TArray<FHktEntityId> DummyEntities;
};

// ============================================================================
// Player State
// ============================================================================

struct HKTCORE_API FHktPlayerState
{
    TArray<FHktEvent> ActiveEvents;
    TArray<FHktEntityState> EntityStates;

    friend HKTCORE_API FArchive& operator<<(FArchive& Ar, FHktPlayerState& State)
    {
        Ar << State.ActiveEvents;
        Ar << State.EntityStates;
        return Ar;
    }
};
