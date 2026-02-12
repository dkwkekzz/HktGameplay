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
    FGameplayTagContainer Tags;

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
        // TODO: 직렬화 지원이 안됨.
        //Ar << State.Tags;       // FGameplayTagContainer 자체 직렬화 지원
        Ar << State.Properties; // TArray<int32> 자체 직렬화 지원
        return Ar;
    }
};

/** 시뮬레이션의 전체 스냅샷 (Deep Copy 및 Rollback 지원 필수) */
struct HKTCORE_API FHktWorldState
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;

    /** 다음 엔티티 할당 시 사용할 ID */
    FHktEntityId NextEntityId = 0;

    // Entity Storage
    TMap<FHktEntityId, FHktEntityState> Entities;

    // TODO: ActiveEvents가 있어야 할 거 같음.

    // Helpers
    FHktEntityState* GetEntityMutable(FHktEntityId Id) { return Entities.Find(Id); }
    const FHktEntityState* GetEntity(FHktEntityId Id) const { return Entities.Find(Id); }
    void RemoveEntity(FHktEntityId Id) { Entities.Remove(Id); }

    /** 새 엔티티를 할당하고 ID를 반환 */
    FHktEntityId AllocateEntity()
    {
        FHktEntityId NewId = NextEntityId++;
        FHktEntityState& State = Entities.Add(NewId);
        State.EntityId = NewId;
        return NewId;
    }

    bool IsValidEntity(FHktEntityId Id) const { return Entities.Contains(Id); }

    void CopyFrom(const FHktWorldState& Other)
    {
        FrameNumber = Other.FrameNumber;
        RandomSeed = Other.RandomSeed;
        NextEntityId = Other.NextEntityId;
        Entities = Other.Entities;
    }

    // 직렬화 연산자 오버로딩
    // TMap<Key, Value>는 Key와 Value에 << 연산자가 있으면 자동으로 직렬화됩니다.
    friend FArchive& operator<<(FArchive& Ar, FHktWorldState& WorldState)
    {
        Ar << WorldState.FrameNumber;
        Ar << WorldState.RandomSeed;
        Ar << WorldState.NextEntityId;
        Ar << WorldState.Entities;
        return Ar;
    }
};

/** 렌더링용 보간 상태 */
struct HKTCORE_API FHktRenderState
{
    int64 FrameNumber = 0;
    TArray<FHktEntityState> InterpolatedEntities;
};
