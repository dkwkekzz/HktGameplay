// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "HktRuntimeTypes.generated.h"

/**
 * 엔티티 스냅샷 - 엔티티의 전체 상태를 직렬화
 * 
 * Properties: 숫자 데이터 (위치, 체력 등)
 * Tags: 모든 태그 (Visual, Flow, EntityType, Status 등)
 */
USTRUCT(BlueprintType)
struct HKTRUNTIME_API FHktEntitySnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    int32 EntityId = InvalidEntityId;

    /** 숫자 Property 배열 (PropertyId = 인덱스) */
    UPROPERTY()
    TArray<int32> Properties;

    /** 엔티티의 모든 태그 */
    UPROPERTY()
    FGameplayTagContainer Tags;

    bool IsValid() const { return EntityId != InvalidEntityId; }
    FHktEntityId GetEntityId() const { return EntityId; }
    
    /** 특정 태그 존재 여부 */
    bool HasTag(const FGameplayTag& Tag) const { return Tags.HasTag(Tag); }
    
    /** 태그 매칭 (부모 태그도 매칭) */
    bool HasTagExact(const FGameplayTag& Tag) const { return Tags.HasTagExact(Tag); }
};

/**
 * [Intent Event]
 * Represents an incident or event in the world.
 * Can be an input action, a state change, or an entity existence.
 */
USTRUCT(BlueprintType)
struct HKTRUNTIME_API FHktRuntimeEvent
{
	GENERATED_BODY()

	FHktRuntimeEvent()
		: EventId(0)
		, SourceEntityId(InvalidEntityId)
		, TargetEntityId(InvalidEntityId)
	{}

	// Unique ID of the event
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EventId;

	// The Source/Subject of this event (Relevancy 계산 기준)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SourceEntityId;

	// Classification of the event (What happened)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTag EventTag;

	// The Target entity involved
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 TargetEntityId;

	// Location data (if applicable)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Location = FVector::ZeroVector;

    // 추가 파라미터
    UPROPERTY(BlueprintReadWrite)
    TArray<uint8> Payload;

    bool operator==(const FHktRuntimeEvent& Other) const
    {
        return EventId == Other.EventId;
    }

    bool operator!=(const FHktRuntimeEvent& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FHktRuntimeEvent& Other) const
    {
        return EventId < Other.EventId;
	}

	bool IsValid() const 
	{ 
		return EventId != 0; 
	}
};

//=============================================================================
// FHktRuntimeBatch - 기존 유저용 "입력(Input)" 패킷
//=============================================================================
USTRUCT()
struct HKTRUNTIME_API FHktRuntimeBatch
{
    GENERATED_BODY()

    UPROPERTY()
    int64 FrameNumber = 0;

    UPROPERTY()
    int32 RandomSeed = 0;

    UPROPERTY()
    float DeltaSeconds = 0.0f;

    UPROPERTY()
    TArray<int64> RemovedOwnerIds;

    UPROPERTY()
    TArray<FHktRuntimeEvent> Events;

    void Reset()
    {
        FrameNumber = 0;
        RandomSeed = 0;
        DeltaSeconds = 0.0f;
        RemovedOwnerIds.Reset();
        Events.Reset();
    }
};

//=============================================================================
// FHktRuntimeSimulationState - 신규 유저용 "결과(Result)" 패킷
//=============================================================================
// [중요] 그룹의 시뮬레이션 결과를 완벽하게 복원하기 위한 모든 데이터를 포함해야 함
USTRUCT()
struct HKTRUNTIME_API FHktRuntimeSimulationState
{
    GENERATED_BODY()

    UPROPERTY()
    int64 LastProcessedFrameNumber = 0; // 이 상태가 어떤 프레임까지 반영된 결과인지

    // 엔티티들의 최종 스냅샷
    UPROPERTY()
    TArray<FHktEntitySnapshot> EntitySnapshots;

    // [결정론 보장] 현재 진행 중인, 아직 만료되지 않은 지속성 이벤트나 상태
    // 예: 쿨타임 정보, 날씨 상태, 현재 RNG의 내부 상태값 등
    UPROPERTY()
    TArray<FHktRuntimeEvent> ActiveEvents;
};

//=============================================================================
// FHktRuntimeOwnerState - 임의의 플레이어의 시뮬레이션 결과
//=============================================================================
USTRUCT()
struct HKTRUNTIME_API FHktRuntimeOwnerState
{
    GENERATED_BODY()

    // 엔티티들의 최종 스냅샷
    UPROPERTY()
    TArray<FHktEntitySnapshot> EntitySnapshots;

    // [결정론 보장] 현재 진행 중인, 아직 만료되지 않은 지속성 이벤트나 상태
    // 예: 쿨타임 정보, 날씨 상태, 현재 RNG의 내부 상태값 등
    UPROPERTY()
    TArray<FHktRuntimeEvent> ActiveEvents;
};
