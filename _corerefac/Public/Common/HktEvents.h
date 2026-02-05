// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Common/HktTypes.h"
#include "HktEvents.generated.h"

// ============================================================================
// [Part 1] Cell Change Event
// ============================================================================

/**
 * FHktCellChangeEvent - 엔티티의 셀 변경 이벤트
 *
 * 엔티티가 위치 변경으로 다른 셀로 이동하거나,
 * 생성/삭제될 때 발생
 */
USTRUCT()
struct HKTCORE_API FHktCellChangeEvent
{
    GENERATED_BODY()

    UPROPERTY()
    FHktEntityId Entity = InvalidEntityId;

    UPROPERTY()
    FIntPoint OldCell = InvalidCell;

    UPROPERTY()
    FIntPoint NewCell = InvalidCell;

    /** 엔티티 생성 (OldCell = Invalid) */
    bool IsEnter() const { return OldCell == InvalidCell && NewCell != InvalidCell; }

    /** 엔티티 삭제 (NewCell = Invalid) */
    bool IsExit() const { return OldCell != InvalidCell && NewCell == InvalidCell; }

    /** 셀 간 이동 */
    bool IsMove() const { return OldCell != InvalidCell && NewCell != InvalidCell && OldCell != NewCell; }
};

// ============================================================================
// [Part 2] Entity Snapshot
// ============================================================================

/**
 * 엔티티 스냅샷 - 엔티티의 전체 상태를 직렬화
 *
 * Properties: 숫자 데이터 (위치, 체력 등)
 * Tags: 모든 태그 (Visual, Flow, EntityType, Status 등)
 */
USTRUCT(BlueprintType)
struct HKTCORE_API FHktEntitySnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    FHktEntityId EntityId = InvalidEntityId;

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

// ============================================================================
// [Part 3] Intent Event (External/Networked)
// ============================================================================

/**
 * FHktIntentEvent - 외부 입력 이벤트
 *
 * 출처: PlayerController -> HktUnreal -> HktCore
 * 네트워크 리플리케이션 대상 (직렬화 비용 발생)
 * 용도: 이동 명령, 스킬 시전, 아이템 사용 등
 */
USTRUCT(BlueprintType)
struct HKTCORE_API FHktIntentEvent
{
    GENERATED_BODY()

    FHktIntentEvent()
        : EventId(0)
        , SourceEntity(InvalidEntityId)
        , TargetEntity(InvalidEntityId)
        , bIsGlobal(false)
    {}

    // Unique ID of the event
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 EventId;

    // The Source/Subject of this event (Relevancy 계산 기준)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FHktEntityId SourceEntity;

    // Classification of the event (What happened)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FGameplayTag EventTag;

    // The Target entity involved
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FHktEntityId TargetEntity;

    // Location data (if applicable)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FVector Location = FVector::ZeroVector;

    // 추가 파라미터
    UPROPERTY(BlueprintReadWrite)
    TArray<uint8> Payload;

    // 글로벌 이벤트 여부 (true면 모든 클라이언트에게 전송)
    UPROPERTY(BlueprintReadWrite)
    bool bIsGlobal = false;

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

// ============================================================================
// [Part 4] System Event (Internal/Local) — NEW
// ============================================================================

/**
 * FHktSystemEvent - 내부 시스템 이벤트
 *
 * 출처: 시뮬레이션 내부 로직 (물리 충돌, 타이머 만료, AI 트리거)
 * 네트워크 전송 X, 로컬에서 즉시 소비됨 (직렬화 비용 최소화)
 * 용도: 충돌 반응(넉백/피격), 쿨타임 종료 알림, 연쇄 폭발 등
 */
struct HKTCORE_API FHktSystemEvent
{
    /** 이벤트 분류 (프로그램 매칭 키) */
    FGameplayTag EventTag;

    /** 이벤트 발생 주체 */
    FHktEntityId SourceEntity = InvalidEntityId;

    /** 이벤트 대상 */
    FHktEntityId TargetEntity = InvalidEntityId;

    /** 위치 데이터 (선택적) */
    FVector Location = FVector::ZeroVector;

    /** 범용 정수 파라미터 */
    int32 Param0 = 0;
    int32 Param1 = 0;
};

// ============================================================================
// [Part 5] Frame Batch (Server → Client)
// ============================================================================

/**
 * FHktFrameBatch - 서버 → 클라이언트 프레임 배치
 *
 * 스냅샷과 이벤트를 분리하여 전송
 * - Snapshots: Relevancy에 새로 진입한 엔티티들
 * - Events: 이번 프레임의 Intent들
 */
USTRUCT()
struct HKTCORE_API FHktFrameBatch
{
    GENERATED_BODY()

    UPROPERTY()
    int32 FrameNumber = 0;

    // Relevancy에 새로 진입한 엔티티 스냅샷
    UPROPERTY()
    TArray<FHktEntitySnapshot> Snapshots;

    // Relevancy를 벗어난 엔티티 ID (클라가 제거해야 함)
    UPROPERTY()
    TArray<FHktEntityId> RemovedEntities;

    // 이번 프레임의 이벤트들 (스냅샷 분리됨)
    UPROPERTY()
    TArray<FHktIntentEvent> Events;

    int32 NumEvents() const { return Events.Num(); }
    int32 NumSnapshots() const { return Snapshots.Num(); }
    bool IsEmpty() const { return Events.IsEmpty() && Snapshots.IsEmpty() && RemovedEntities.IsEmpty(); }

    void Reset()
    {
        FrameNumber = 0;
        Snapshots.Empty();
        RemovedEntities.Empty();
        Events.Empty();
    }
};
