// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreMinimal.h"
#include "GameplayTagContainer.h"

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
// FHktSimulationEvent — 프레임 단위 시뮬레이션 입력
// ============================================================================

struct HKTCORE_API FHktSimulationEvent
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    float DeltaSeconds = 0.0f;
    TArray<int64> RemovedOwnerIds;
    TArray<FHktEvent> Events;

    FString ToString() const
    {
        return FString::Printf(TEXT("Frame=%lld Seed=%d Dt=%.3f Removed=%d Events=%d"),
            FrameNumber, RandomSeed, DeltaSeconds, RemovedOwnerIds.Num(), Events.Num());
    }

    void Reset()
    {
        FrameNumber = 0; RandomSeed = 0; DeltaSeconds = 0.0f;
        RemovedOwnerIds.Reset(); Events.Reset();
    }

    friend FArchive& operator<<(FArchive& Ar, FHktSimulationEvent& E)
    {
        Ar << E.FrameNumber << E.RandomSeed << E.DeltaSeconds;
        Ar << E.RemovedOwnerIds << E.Events;
        return Ar;
    }
};
