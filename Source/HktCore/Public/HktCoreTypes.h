// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

// ============================================================================
// Basic Types & IDs
// ============================================================================

using FHktEntityId = int32;
constexpr FHktEntityId InvalidEntityId = -1;

/** 범용 게임플레이 이벤트 */
struct HKTCORE_API FHktEvent
{
    FGameplayTag EventTag;
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    FVector Location = FVector::ZeroVector;
    int32 Param0 = 0;
    int32 Param1 = 0;
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
};

/** 렌더링용 보간 상태 */
struct HKTCORE_API FHktRenderState
{
    int64 FrameNumber = 0;
    TArray<FHktEntityState> InterpolatedEntities;
};
