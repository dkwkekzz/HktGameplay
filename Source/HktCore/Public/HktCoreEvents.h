// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreDefs.h"
#include "GameplayTagContainer.h"

static FArchive& operator<<(FArchive& Ar, FGameplayTagContainer& Tags)
{
    bool bSuccess = false;
    Tags.NetSerialize(Ar, nullptr, bSuccess);
    return Ar;
}

// ============================================================================
// Inline Payload — 이벤트 내 가변 데이터 컨테이너
// ============================================================================

struct FHktInlinePayload
{
    static constexpr int32 Capacity = 48;
    uint8 Data[Capacity] = { 0 };
    uint8 Size = 0;

    FHktInlinePayload() = default;

    template <typename... Args>
    void Set(Args... InArgs) { Size = 0; (Write(InArgs), ...); }

    template <typename T>
    void Write(const T& Value)
    {
        static_assert(TIsBitwiseConstructible<T>::Value, "Only bitwise-copyable types.");
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
        if (P.Size > 0) Ar.Serialize(P.Data, P.Size);
        return Ar;
    }
};

// ============================================================================
// FHktEvent — 범용 게임플레이 이벤트
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

    FString ToString() const
    {
        return FString::Printf(TEXT("EventId=%d Tag=%s Src=%d Tgt=%d"),
            EventId, *EventTag.ToString(), SourceEntity, TargetEntity);
    }

    friend FArchive& operator<<(FArchive& Ar, FHktEvent& E)
    {
        Ar << E.EventId << E.SourceEntity << E.EventTag;
        Ar << E.TargetEntity << E.Location << E.Param0 << E.Param1;
        return Ar;
    }
};

// ============================================================================
// FHktEntityState — 엔티티 DTO (직렬화 / Diff / DB)
//
// Data는 LocalIndex 순서. size = Stride. memcpy로 풀과 교환 가능.
// ============================================================================

struct HKTCORE_API FHktEntityState
{
    FHktEntityId EntityId = InvalidEntityId;
    FHktTypeId TypeId = HktType::None;
    TArray<int32> Data;
    FGameplayTagContainer Tags;

    friend FArchive& operator<<(FArchive& Ar, FHktEntityState& S)
    {
        Ar << S.EntityId << S.TypeId << S.Data << S.Tags;
        return Ar;
    }
};

// ============================================================================
// FHktSimulationEvent — 프레임 단위 시뮬레이션 입력
// ============================================================================

struct HKTCORE_API FHktSimulationEvent
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    float DeltaSeconds = 0.0f;
    TArray<int64> RemovedOwnerIds;
    TArray<FHktEvent> NewEvents;
    TArray<FHktEntityState> NewEntityStates;  // ← 추가: 신규 진입자 엔티티

    FString ToString() const
    {
        return FString::Printf(TEXT("Frame=%lld Seed=%d Dt=%.3f Removed=%d Events=%d NewStates=%d"),
            FrameNumber, RandomSeed, DeltaSeconds, RemovedOwnerIds.Num(), NewEvents.Num(), NewEntityStates.Num());
    }

    void Reset()
    {
        FrameNumber = 0; RandomSeed = 0; DeltaSeconds = 0.0f;
        RemovedOwnerIds.Reset(); NewEvents.Reset();
        NewEntityStates.Reset();  // ← 추가
    }

    friend FArchive& operator<<(FArchive& Ar, FHktSimulationEvent& E)
    {
        Ar << E.FrameNumber << E.RandomSeed << E.DeltaSeconds;
        Ar << E.RemovedOwnerIds << E.NewEvents;
        Ar << E.NewEntityStates;  // ← 추가
        return Ar;
    }
};

// ============================================================================
// FHktPropertyDelta — 단일 프로퍼티 변경
// ============================================================================

struct HKTCORE_API FHktPropertyDelta
{
    FHktEntityId EntityId = InvalidEntityId;
    uint16 PropertyId = 0;
    int32 NewValue = 0;
    int32 OldValue = 0;

    friend FArchive& operator<<(FArchive& Ar, FHktPropertyDelta& D)
    {
        Ar << D.EntityId << D.PropertyId << D.NewValue << D.OldValue;
        return Ar;
    }
};

// ============================================================================
// FHktTagDelta — 엔티티 태그 변경 스냅샷 (서버 → 클라이언트)
// ============================================================================

struct HKTCORE_API FHktTagDelta
{
    FHktEntityId EntityId = InvalidEntityId;
    FGameplayTagContainer Tags;
    FGameplayTagContainer OldTags;

    friend FArchive& operator<<(FArchive& Ar, FHktTagDelta& D)
    {
        Ar << D.EntityId << D.Tags << D.OldTags;
        return Ar;
    }
};

// ============================================================================
// FHktSimulationDiff — 프레임별 변경점 (서버 → 클라이언트)
// ============================================================================

struct HKTCORE_API FHktSimulationDiff
{
    int64 FrameNumber = 0;
    TArray<FHktEntityState> SpawnedEntities;
    TArray<FHktEntityId> RemovedEntities;
    TArray<FHktEntityState> RemovedEntityStates;  // 제거된 엔티티 전체 상태 (UndoDiff 복원용)
    TArray<FHktPropertyDelta> PropertyDeltas;
    TArray<FHktTagDelta> TagDeltas;
    FHktEntityId PrevNextEntityId = InvalidEntityId;  // 이 프레임 실행 전 NextEntityId (Undo 시 복원)

    friend FArchive& operator<<(FArchive& Ar, FHktSimulationDiff& D)
    {
        Ar << D.FrameNumber << D.SpawnedEntities << D.RemovedEntities
           << D.RemovedEntityStates << D.PropertyDeltas << D.TagDeltas << D.PrevNextEntityId;
        return Ar;
    }
};

// ============================================================================
// FHktPlayerState — 플레이어 단위 상태 (그룹 이동 / DB 저장)
// ============================================================================

struct HKTCORE_API FHktPlayerState
{
    int64 PlayerUid = 0;
    TArray<FHktEntityState> OwnedEntities;
    TArray<FHktEvent> ActiveEvents;

    friend FArchive& operator<<(FArchive& Ar, FHktPlayerState& S)
    {
        Ar << S.PlayerUid << S.OwnedEntities << S.ActiveEvents;
        return Ar;
    }
};
