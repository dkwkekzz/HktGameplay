// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPlayerDatabaseComponent.h"
#include "HktFilePlayerDataProvider.h"

UHktPlayerDatabaseComponent::UHktPlayerDatabaseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    Provider = MakeUnique<FHktFilePlayerDataProvider>();

    DefaultVisualTag = FGameplayTag::RequestGameplayTag(TEXT("Visual.Character.Default"), false);
    DefaultFlowTag = FGameplayTag::RequestGameplayTag(TEXT("Flow.Character.Default"), false);
}

void UHktPlayerDatabaseComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UHktPlayerDatabaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 종료 시 캐시된 레코드 저장
    for (const auto& Pair : CachedRecords)
    {
        Provider->Save(FString::FromInt(Pair.Key), Pair.Value, [](bool) {});
    }
    Super::EndPlay(EndPlayReason);
}

// ============================================================================
// IHktWorldDatabase 구현
// ============================================================================

void UHktPlayerDatabaseComponent::LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(TUniquePtr<FHktPlayerRecord>)> InCallback)
{
    // 캐시 확인
    if (FHktPlayerRecord* Cached = CachedRecords.Find(InPlayerUid))
    {
        InCallback(MakeUnique<FHktPlayerRecord>(*Cached));
        return;
    }

    FString PlayerIdStr = FString::FromInt(InPlayerUid);

    Provider->Load(PlayerIdStr, [this, InPlayerUid, InCallback](TOptional<FHktPlayerRecord> Loaded)
    {
        if (Loaded.IsSet())
        {
            FHktPlayerRecord& Record = Loaded.GetValue();
            Record.PlayerUid = InPlayerUid;
            CachedRecords.Add(InPlayerUid, Record);
            InCallback(MakeUnique<FHktPlayerRecord>(MoveTemp(Record)));
        }
        else
        {
            // 신규 플레이어: 기본 레코드 생성
            FHktPlayerRecord NewRecord;
            NewRecord.PlayerUid = InPlayerUid;
            NewRecord.CreatedTime = FDateTime::UtcNow();
            NewRecord.LastLoginTime = NewRecord.CreatedTime;

            CachedRecords.Add(InPlayerUid, NewRecord);
            InCallback(MakeUnique<FHktPlayerRecord>(MoveTemp(NewRecord)));
        }
    });
}

void UHktPlayerDatabaseComponent::SavePlayerRecordAsync(FHktPlayerRecord InRecord)
{
    int64 Uid = InRecord.PlayerUid;
    CachedRecords.Add(Uid, InRecord);

    FString PlayerIdStr = FString::FromInt(Uid);
    Provider->Save(PlayerIdStr, InRecord, [Uid](bool bSuccess)
    {
        if (!bSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("[PlayerDatabase] Save failed for PlayerUid=%lld"), Uid);
        }
    });
}
