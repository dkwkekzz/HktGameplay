// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktTransientDatabaseComponent.h"
#include "GameplayTagContainer.h"
#include "HktStoryTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktTransientDatabase, Log, All);

UHktTransientDatabaseComponent::UHktTransientDatabaseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    DefaultVisualTag = FGameplayTag::RequestGameplayTag(TEXT("Visual.Character.Default"), false);
    DefaultFlowTag = FGameplayTag::RequestGameplayTag(TEXT("Flow.Character.Default"), false);
}

// ============================================================================
// IHktWorldDatabase 구현
// ============================================================================

void UHktTransientDatabaseComponent::LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(const FHktPlayerRecord&)> InCallback)
{
    // 메모리에서 레코드 찾기
    if (FHktPlayerRecord* Cached = TransientRecords.Find(InPlayerUid))
    {
        InCallback(*Cached);
        return;
    }

    // 레코드가 없으면 새로 생성
    FHktPlayerRecord& NewRecord = TransientRecords.Add(InPlayerUid);
    NewRecord.PlayerUid = InPlayerUid;
    NewRecord.CreatedTime = FDateTime::UtcNow();
    NewRecord.LastLoginTime = NewRecord.CreatedTime;
    
    // 신규 플레이어 레코드에 월드 진입 이벤트 추가
    FHktEvent EnterWorldEvent;
    EnterWorldEvent.EventTag = HktStoryTags::Story_PlayerInWorld;
    EnterWorldEvent.SourceEntity = static_cast<FHktEntityId>(InPlayerUid); // 임시, 플로우에서 실제 엔티티 생성
    EnterWorldEvent.TargetEntity = InvalidEntityId;
    EnterWorldEvent.Location = FVector::ZeroVector; // 기본 스폰 위치는 여기서 정하는게 아님
    EnterWorldEvent.PlayerUid = InPlayerUid;
    NewRecord.ActiveEvents.Add(EnterWorldEvent);

    InCallback(NewRecord);
}

void UHktTransientDatabaseComponent::SavePlayerRecordAsync(int64 InPlayerUid, FHktPlayerState&& InState)
{
    // 기존 레코드 로드 또는 새로 생성
    FHktPlayerRecord* ExistingRecord = TransientRecords.Find(InPlayerUid);
    
    if (ExistingRecord)
    {
        // 기존 레코드 업데이트: ActiveEvents와 EntityStates만 이동
        ExistingRecord->ActiveEvents = MoveTemp(InState.ActiveEvents);
        ExistingRecord->EntityStates = MoveTemp(InState.OwnedEntities);
        // LastLoginTime, CreatedTime, LastPosition은 유지
    }
    else
    {
        // 신규 레코드 생성
        FHktPlayerRecord NewRecord;
        NewRecord.PlayerUid = InPlayerUid;
        NewRecord.CreatedTime = FDateTime::UtcNow();
        NewRecord.LastLoginTime = NewRecord.CreatedTime;
        NewRecord.LastPosition = FVector::ZeroVector;
        NewRecord.ActiveEvents = MoveTemp(InState.ActiveEvents);
        NewRecord.EntityStates = MoveTemp(InState.OwnedEntities);
        
        TransientRecords.Add(InPlayerUid, MoveTemp(NewRecord));
    }
    
    UE_LOG(LogHktTransientDatabase, VeryVerbose, TEXT("[TransientDatabase] Saved player record in memory: PlayerUid=%lld"), InPlayerUid);
}

const FHktPlayerRecord* UHktTransientDatabaseComponent::GetCachedPlayerRecord(int64 InPlayerUid) const
{
    return TransientRecords.Find(InPlayerUid);
}