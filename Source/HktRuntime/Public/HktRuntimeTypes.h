// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"

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
    int32 EntityId = INDEX_NONE;

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
struct HKTRUNTIME_API FHktIntentEvent
{
	GENERATED_BODY()

	FHktIntentEvent()
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

    bool operator==(const FHktIntentEvent& Other) const
    {
        return EventId == Other.EventId;
    }

    bool operator!=(const FHktIntentEvent& Other) const
    {
        return !(*this == Other);
    }

    bool operator<(const FHktIntentEvent& Other) const
    {
        return EventId < Other.EventId;
	}

	bool IsValid() const 
	{ 
		return EventId != 0; 
	}
};

/**
 * FHktFrameBatch - 서버 → 클라이언트 프레임 배치
 * 
 * 스냅샷과 이벤트를 분리하여 전송
 * - Snapshots: Relevancy에 새로 진입한 엔티티들
 * - Events: 이번 프레임의 Intent들
 */
USTRUCT()
struct HKTRUNTIME_API FHktFrameBatch
{
    GENERATED_BODY()

    UPROPERTY()
    int64 FrameNumber = 0;

    // Relevancy에 새로 진입한 엔티티 스냅샷
    UPROPERTY()
    TArray<FHktEntitySnapshot> Snapshots;

    // Relevancy를 벗어난 엔티티 ID (클라가 제거해야 함)
    UPROPERTY()
    TArray<int32> RemovedEntityIds;

    // 이번 프레임의 이벤트들 (스냅샷 분리됨)
    UPROPERTY()
    TArray<FHktIntentEvent> Events;

    int32 NumEvents() const { return Events.Num(); }
    int32 NumSnapshots() const { return Snapshots.Num(); }
    bool IsEmpty() const { return Events.IsEmpty() && Snapshots.IsEmpty() && RemovedEntityIds.IsEmpty(); }
    
    void Reset()
    {
        FrameNumber = 0;
        Snapshots.Empty();
        RemovedEntityIds.Empty();
        Events.Empty();
    }
};
