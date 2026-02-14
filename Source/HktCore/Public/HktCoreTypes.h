// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

// ============================================================================
// Basic Types & IDs
// ============================================================================

using FHktEntityId = int32;
constexpr FHktEntityId InvalidEntityId = -1;

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
        // [수정됨] TIsTriviallyCopyable 대신 언리얼 표준인 TIsBitwiseConstructible 사용
        // FVector 등 언리얼 수학 타입들은 생성자가 있어도 Bitwise Copy가 안전하다고 엔진에 정의되어 있음
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

    // [핵심] FArchive 직렬화 연산자 오버로딩
    // 이것 덕분에 TArray<FHktEvent> 내부에서 Payload를 만났을 때 자동으로 처리됩니다.
    friend FArchive& operator<<(FArchive& Ar, FHktInlinePayload& Payload)
    {
        Ar << Payload.Size;
        if (Payload.Size > 0)
        {
            Ar.Serialize(Payload.Data, Payload.Size);
        }
        return Ar;
    }
};

/** 범용 게임플레이 이벤트 */
struct HKTCORE_API FHktEvent
{
    int32 EventId = 0;
    FGameplayTag EventTag;
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    FVector Location = FVector::ZeroVector;
    int32 Param0 = 0;
    int32 Param1 = 0;

    // [핵심] FArchive 직렬화 연산자 오버로딩
    // 이 친구가 있으면 TArray<FHktEvent>를 Ar << Events; 한 줄로 보낼 수 있습니다.
    friend FArchive& operator<<(FArchive& Ar, FHktEvent& Event)
    {
        Ar << Event.EventId;
        Ar << Event.SourceEntity;
        Ar << Event.EventTag;
        Ar << Event.TargetEntity;
        Ar << Event.Location;
        Ar << Event.Param0;
        Ar << Event.Param1;
        return Ar;
    }
};

/** 물리 충돌 이벤트 */
struct HKTCORE_API FHktPhysicsEvent
{
    FHktEntityId EntityA = InvalidEntityId;
    FHktEntityId EntityB = InvalidEntityId;
    FVector ContactPoint = FVector::ZeroVector;
};

/** 프레임 단위 시뮬레이션 입력 */
struct HKTCORE_API FHktSimulationEvent
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    float DeltaSeconds = 0.0f;
    TArray<int64> RemovedOwnerIds;
    TArray<FHktEvent> Events;

    void Reset()
    {
        FrameNumber = 0;
        RandomSeed = 0;
        DeltaSeconds = 0.0f;
        RemovedOwnerIds.Reset();
        Events.Reset();
    }

    // SimulationEvent 자체도 중첩 구조로 사용될 수 있으므로 연산자를 정의합니다.
    friend FArchive& operator<<(FArchive& Ar, FHktSimulationEvent& SimEvent)
    {
        Ar << SimEvent.FrameNumber;
        Ar << SimEvent.RandomSeed;
        Ar << SimEvent.DeltaSeconds;
        Ar << SimEvent.RemovedOwnerIds;
        Ar << SimEvent.Events; // TArray가 내부 요소의 operator<<를 자동으로 호출함
        return Ar;
    }
};

// ============================================================================
// Entity & World State
// ============================================================================

struct HKTCORE_API FHktEntityState
{
    FHktEntityId EntityId = InvalidEntityId;
    FVector Position = FVector::ZeroVector;
    TArray<int32> TagIndices;

    // VM은 int32 단위로 Property를 읽고 쓰므로 int32 배열로 관리
    TArray<int32> Properties;

    /** PropertyId 인덱스로 값 읽기 (범위 밖이면 0) */
    int32 GetProperty(uint16 PropertyId) const
    {
        return (PropertyId < static_cast<uint16>(Properties.Num())) ? Properties[PropertyId] : 0;
    }

    /** PropertyId 인덱스로 값 쓰기 (필요 시 배열 확장) */
    void SetProperty(uint16 PropertyId, int32 Value)
    {
        if (PropertyId >= static_cast<uint16>(Properties.Num()))
        {
            Properties.SetNumZeroed(PropertyId + 1);
        }
        Properties[PropertyId] = Value;
    }

    // 직렬화 연산자 오버로딩
    friend FArchive& operator<<(FArchive& Ar, FHktEntityState& State)
    {
        Ar << State.EntityId;
        Ar << State.Position;
        Ar << State.TagIndices;
        Ar << State.Properties;
        return Ar;
    }
};

/** SOA 데이터 컬럼 — PropertyId별 int32 배열 */
struct HKTCORE_API FHktDataColumn
{
    int32 PropertyId = -1;
    TArray<int32> IntData;

    void Resize(int32 Size) { IntData.SetNum(Size); }
    void SetZeroed(int32 Size) { IntData.SetNumZeroed(Size); }

    int32 GetInt(int32 Index) const { return IntData.IsValidIndex(Index) ? IntData[Index] : 0; }
    void SetInt(int32 Index, int32 Value) { if (IntData.IsValidIndex(Index)) IntData[Index] = Value; }

    friend FArchive& operator<<(FArchive& Ar, FHktDataColumn& Col)
    {
        Ar << Col.PropertyId;
        Ar << Col.IntData;
        return Ar;
    }
};

/** 시뮬레이션의 전체 스냅샷 — SOA 기반 (Deep Copy 및 Rollback 지원 필수) */
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

    // --- SOA Data Columns (PropertyId -> Column) ---
    TMap<int32, FHktDataColumn> Columns;
    TArray<TArray<int32>> TagColumn;  // SlotIndex -> TagIndices

    // --- Active Events (진행 중인 이벤트 — 중간 합류 클라이언트 동기화용) ---
    TArray<FHktEvent> ActiveEvents;

    // --- Core Operations ---
    FHktEntityId AllocateEntity();
    void RemoveEntity(FHktEntityId Id);

    bool IsValidEntity(FHktEntityId Id) const
    {
        return Id >= 0 && Id < EntityToIndex.Num() && EntityToIndex[Id] != -1;
    }

    int32 GetIndex(FHktEntityId Id) const
    {
        return IsValidEntity(Id) ? EntityToIndex[Id] : -1;
    }

    int32 GetEntityCount() const
    {
        return IndexToEntity.Num() - FreeIndices.Num();
    }

    // --- Property Access ---
    int32 GetProperty(FHktEntityId Entity, uint16 PropertyId) const;
    void SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value);

    // --- Column Access ---
    const FHktDataColumn* GetColumn(int32 PropertyId) const { return Columns.Find(PropertyId); }
    FHktDataColumn& GetOrCreateColumn(int32 PropertyId);

    // --- Iteration (template — 헤더에 유지) ---
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

    // --- Compatibility: Entity 추출 (HktRuntime DTO 변환용) ---
    FHktEntityState ExtractEntityState(FHktEntityId Id) const;

    // --- Snapshot/Rollback ---
    void CopyFrom(const FHktWorldState& Other);

    // --- 직렬화 ---
    friend HKTCORE_API FArchive& operator<<(FArchive& Ar, FHktWorldState& WorldState);
};

// ============================================================================
// World View (Zero-Copy Interface)
// ============================================================================

/** 로컬 오버레이 단일 항목 (정렬됨) */
struct HKTCORE_API FHktOverlayEntry
{
    int32 PropertyId;
    FHktEntityId EntityId;
    int32 Value;

    // 정렬 및 이진 탐색을 위한 비교 연산자
    bool operator<(const FHktOverlayEntry& Other) const
    {
        if (PropertyId != Other.PropertyId) return PropertyId < Other.PropertyId;
        return EntityId < Other.EntityId;
    }
};

/** * [Zero-Copy View]
 * 렌더러가 상태를 조회하기 위한 인터페이스입니다.
 * - 단일 Array로 평탄화된 오버레이를 사용하여 생성 비용을 최소화합니다.
 */
struct HKTCORE_API FHktWorldView
{
    // 원본 데이터 참조
    const FHktWorldState* WorldState = nullptr;

    // 로컬 오버레이 (Active VM Context) - Int32 Only
    // [최적화] PublishRenderSystem에서 PropertyId -> EntityId 순으로 정렬(Sort)하여 채웁니다.
    TArray<FHktOverlayEntry> IntOverlays;

    // --- Accessors ---

    int32 GetValue(FHktEntityId Entity, int32 PropertyId) const
    {
        // 1. Check Local Overlay (Binary Search for O(log N))
        // 오버레이는 정렬되어 있으므로 이진 탐색 사용 가능
        // (직접 구현 혹은 Algo::BinarySearchBy 사용)
        int32 Low = 0;
        int32 High = IntOverlays.Num() - 1;

        while (Low <= High)
        {
            int32 Mid = (Low + High) / 2;
            const FHktOverlayEntry& Entry = IntOverlays[Mid];

            if (Entry.PropertyId < PropertyId)
            {
                Low = Mid + 1;
            }
            else if (Entry.PropertyId > PropertyId)
            {
                High = Mid - 1;
            }
            else // PropertyId Match
            {
                if (Entry.EntityId < Entity) Low = Mid + 1;
                else if (Entry.EntityId > Entity) High = Mid - 1;
                else return Entry.Value; // Found
            }
        }

        // 2. Check Committed World State
        if (WorldState)
        {
            if (const FHktDataColumn* Col = WorldState->GetColumn(PropertyId))
            {
                int32 Index = WorldState->GetIndex(Entity);
                if (Index != -1) return Col->GetInt(Index);
            }
        }
        return 0;
    }

    /** [Full Iteration]
     * 특정 프로퍼티에 대해 모든 엔티티를 순회합니다. (초기화 및 전체 갱신 용도)
     * - Dirty 여부와 상관없이 현재 존재하는 모든 유효한 엔티티를 방문합니다.
     * - WorldState의 IndexToEntity는 Dense Array이므로 순회 효율이 높습니다.
     */
    template<typename Func>
    void ForEachEntity(int32 PropertyId, Func Callback) const
    {
        if (WorldState)
        {
            // 해당 프로퍼티 컬럼이 존재하는지 확인
            if (WorldState->GetColumn(PropertyId))
            {
                for (const FHktEntityId& EntityId : WorldState->IndexToEntity)
                {
                    Callback(EntityId);
                }
            }
        }
    }

    // 전체 엔티티 순회 헬퍼
    const TArray<FHktEntityId>& GetAllEntities() const
    {
        return WorldState ? WorldState->IndexToEntity : DummyEntities;
    }

private:
    static inline TArray<FHktEntityId> DummyEntities;
};
