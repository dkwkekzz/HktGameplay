// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktTransientDatabaseComponent.h"
#include "GameplayTagContainer.h"

UHktTransientDatabaseComponent::UHktTransientDatabaseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    DefaultVisualTag = FGameplayTag::RequestGameplayTag(TEXT("Visual.Character.Default"), false);
    DefaultFlowTag = FGameplayTag::RequestGameplayTag(TEXT("Flow.Character.Default"), false);
}

// ============================================================================
// IHktWorldDatabase 구현
// ============================================================================

void UHktTransientDatabaseComponent::LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(TUniquePtr<FHktPlayerRecord>)> InCallback)
{
    // 메모리에서 레코드 찾기
    if (FHktPlayerRecord* Cached = TransientRecords.Find(InPlayerUid))
    {
        InCallback(MakeUnique<FHktPlayerRecord>(*Cached));
        return;
    }

    // 레코드가 없으면 새로 생성
    FHktPlayerRecord NewRecord;
    NewRecord.PlayerUid = InPlayerUid;
    NewRecord.CreatedTime = FDateTime::UtcNow();
    NewRecord.LastLoginTime = NewRecord.CreatedTime;
    
    // 신규 플레이어 레코드에 월드 진입 이벤트 추가
    FHktEvent EnterWorldEvent;
    EnterWorldEvent.EventTag = FGameplayTag::RequestGameplayTag(TEXT("State.Player.InWorld"), false);
    EnterWorldEvent.SourceEntity = static_cast<FHktEntityId>(InPlayerUid); // 임시, 플로우에서 실제 엔티티 생성
    EnterWorldEvent.TargetEntity = InvalidEntityId;
    EnterWorldEvent.Location = FVector::ZeroVector; // 기본 스폰 위치
    EnterWorldEvent.Param0 = static_cast<int32>(InPlayerUid & 0xFFFFFFFF); // 플레이어 UID 하위 32비트
    EnterWorldEvent.Param1 = static_cast<int32>((InPlayerUid >> 32) & 0xFFFFFFFF); // 플레이어 UID 상위 32비트
    NewRecord.ActiveEvents.Add(EnterWorldEvent);

    TransientRecords.Add(InPlayerUid, NewRecord);
    InCallback(MakeUnique<FHktPlayerRecord>(MoveTemp(NewRecord)));
}

void UHktTransientDatabaseComponent::SavePlayerRecordAsync(FHktPlayerRecord InRecord)
{
    // 메모리에만 저장 (파일 저장 없음)
    int64 Uid = InRecord.PlayerUid;
    TransientRecords.Add(Uid, InRecord);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("[TransientDatabase] Saved player record in memory: PlayerUid=%lld"), Uid);
}
