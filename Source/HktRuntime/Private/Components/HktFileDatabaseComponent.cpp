// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFileDatabaseComponent.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

UHktFileDatabaseComponent::UHktFileDatabaseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    DefaultVisualTag = FGameplayTag::RequestGameplayTag(TEXT("Visual.Character.Default"), false);
    DefaultFlowTag = FGameplayTag::RequestGameplayTag(TEXT("Flow.Character.Default"), false);
}

void UHktFileDatabaseComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UHktFileDatabaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (const auto& Pair : CachedRecords)
    {
        SaveToFile(FString::FromInt(Pair.Key), Pair.Value, [](bool) {});
    }
    Super::EndPlay(EndPlayReason);
}

// ============================================================================
// 파일 경로
// ============================================================================

FString UHktFileDatabaseComponent::SanitizePlayerIdForPath(const FString& PlayerId)
{
    FString Result = PlayerId;
    const TCHAR InvalidChars[] = TEXT("\\/:*?\"<>|");
    for (const TCHAR* C = InvalidChars; *C; ++C)
    {
        Result.ReplaceCharInline(*C, '_');
    }
    return Result.IsEmpty() ? TEXT("_empty") : Result;
}

FString UHktFileDatabaseComponent::GetFilePath(const FString& PlayerId) const
{
    return FPaths::ProjectSavedDir() / TEXT("HktPlayerDatabase") / (SanitizePlayerIdForPath(PlayerId) + TEXT(".json"));
}

// ============================================================================
// 파일 로드 (FHktPlayerRecord: PlayerUid, EntitySnapshots, IntentEvents, ...)
// ============================================================================

void UHktFileDatabaseComponent::LoadFromFile(const FString& PlayerId, TFunction<void(TOptional<FHktPlayerRecord>)> Callback)
{
    FString FilePath = GetFilePath(PlayerId);
    if (!FPaths::FileExists(FilePath))
    {
        Callback(TOptional<FHktPlayerRecord>());
        return;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[FileDatabase] Failed to load file: %s"), *FilePath);
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[FileDatabase] Failed to parse file: %s"), *FilePath);
        return;
    }

    FHktPlayerRecord Record;
    Record.PlayerUid = static_cast<int64>(RootObject->GetNumberField(TEXT("PlayerUid")));
    Record.LastLoginTime = FDateTime::FromUnixTimestamp(static_cast<int64>(RootObject->GetNumberField(TEXT("LastLoginTime"))));
    Record.CreatedTime = FDateTime::FromUnixTimestamp(static_cast<int64>(RootObject->GetNumberField(TEXT("CreatedTime"))));

    const TArray<TSharedPtr<FJsonValue>>* LocArray;
    if (RootObject->TryGetArrayField(TEXT("LastPosition"), LocArray) && LocArray->Num() >= 3)
    {
        Record.LastPosition.X = (*LocArray)[0]->AsNumber();
        Record.LastPosition.Y = (*LocArray)[1]->AsNumber();
        Record.LastPosition.Z = (*LocArray)[2]->AsNumber();
    }

    const TArray<TSharedPtr<FJsonValue>>* EntitiesArray;
    if (RootObject->TryGetArrayField(TEXT("EntitySnapshots"), EntitiesArray))
    {
        for (const TSharedPtr<FJsonValue>& EntityValue : *EntitiesArray)
        {
            const TSharedPtr<FJsonObject>* EntityObject;
            if (!EntityValue->TryGetObject(EntityObject)) continue;

            FHktEntitySnapshot Snapshot;
            Snapshot.EntityId = static_cast<int32>((*EntityObject)->GetNumberField(TEXT("EntityId")));

            const TArray<TSharedPtr<FJsonValue>>* PropsArray;
            if ((*EntityObject)->TryGetArrayField(TEXT("Properties"), PropsArray))
            {
                for (const TSharedPtr<FJsonValue>& P : *PropsArray)
                {
                    Snapshot.Properties.Add(static_cast<int32>(P->AsNumber()));
                }
            }

            const TArray<TSharedPtr<FJsonValue>>* TagsArray;
            if ((*EntityObject)->TryGetArrayField(TEXT("Tags"), TagsArray))
            {
                for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
                {
                    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagValue->AsString()), false);
                    if (Tag.IsValid()) Snapshot.Tags.AddTag(Tag);
                }
            }

            Record.EntitySnapshots.Add(MoveTemp(Snapshot));
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* EventsArray;
    if (RootObject->TryGetArrayField(TEXT("IntentEvents"), EventsArray))
    {
        for (const TSharedPtr<FJsonValue>& EvValue : *EventsArray)
        {
            const TSharedPtr<FJsonObject>* EvObject;
            if (!EvValue->TryGetObject(EvObject)) continue;
            FHktIntentEvent Event;
            Event.EventId = static_cast<int32>((*EvObject)->GetNumberField(TEXT("EventId")));
            Event.SourceEntityId = static_cast<int32>((*EvObject)->GetNumberField(TEXT("SourceEntityId")));
            Event.TargetEntityId = static_cast<int32>((*EvObject)->GetNumberField(TEXT("TargetEntityId")));
            Event.EventTag = FGameplayTag::RequestGameplayTag(FName(*(*EvObject)->GetStringField(TEXT("EventTag"))), false);
            const TArray<TSharedPtr<FJsonValue>>* LocEv;
            if ((*EvObject)->TryGetArrayField(TEXT("Location"), LocEv) && LocEv->Num() >= 3)
            {
                Event.Location.X = (*LocEv)[0]->AsNumber();
                Event.Location.Y = (*LocEv)[1]->AsNumber();
                Event.Location.Z = (*LocEv)[2]->AsNumber();
            }
            Record.IntentEvents.Add(MoveTemp(Event));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[FileDatabase] Loaded player: %s"), *PlayerId);
    Callback(TOptional<FHktPlayerRecord>(MoveTemp(Record)));
}

// ============================================================================
// 파일 저장
// ============================================================================

void UHktFileDatabaseComponent::SaveToFile(const FString& PlayerId, const FHktPlayerRecord& Record, TFunction<void(bool bSuccess)> Callback)
{
    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetNumberField(TEXT("PlayerUid"), static_cast<double>(Record.PlayerUid));
    RootObject->SetNumberField(TEXT("LastLoginTime"), static_cast<double>(Record.LastLoginTime.ToUnixTimestamp()));
    RootObject->SetNumberField(TEXT("CreatedTime"), static_cast<double>(Record.CreatedTime.ToUnixTimestamp()));

    TArray<TSharedPtr<FJsonValue>> LocArray;
    LocArray.Add(MakeShared<FJsonValueNumber>(Record.LastPosition.X));
    LocArray.Add(MakeShared<FJsonValueNumber>(Record.LastPosition.Y));
    LocArray.Add(MakeShared<FJsonValueNumber>(Record.LastPosition.Z));
    RootObject->SetArrayField(TEXT("LastPosition"), LocArray);

    TArray<TSharedPtr<FJsonValue>> EntitiesArray;
    for (const FHktEntitySnapshot& Snapshot : Record.EntitySnapshots)
    {
        TSharedRef<FJsonObject> EntityObject = MakeShared<FJsonObject>();
        EntityObject->SetNumberField(TEXT("EntityId"), Snapshot.EntityId);
        TArray<TSharedPtr<FJsonValue>> PropsArray;
        for (int32 V : Snapshot.Properties)
        {
            PropsArray.Add(MakeShared<FJsonValueNumber>(V));
        }
        EntityObject->SetArrayField(TEXT("Properties"), PropsArray);
        TArray<TSharedPtr<FJsonValue>> TagsArray;
        TArray<FGameplayTag> TagArray;
        Snapshot.Tags.GetGameplayTagArray(TagArray);
        for (const FGameplayTag& Tag : TagArray)
        {
            TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        }
        EntityObject->SetArrayField(TEXT("Tags"), TagsArray);
        EntitiesArray.Add(MakeShared<FJsonValueObject>(EntityObject));
    }
    RootObject->SetArrayField(TEXT("EntitySnapshots"), EntitiesArray);

    TArray<TSharedPtr<FJsonValue>> EventsArray;
    for (const FHktIntentEvent& Event : Record.IntentEvents)
    {
        TSharedRef<FJsonObject> EvObject = MakeShared<FJsonObject>();
        EvObject->SetNumberField(TEXT("EventId"), Event.EventId);
        EvObject->SetNumberField(TEXT("SourceEntityId"), Event.SourceEntityId);
        EvObject->SetNumberField(TEXT("TargetEntityId"), Event.TargetEntityId);
        EvObject->SetStringField(TEXT("EventTag"), Event.EventTag.ToString());
        TArray<TSharedPtr<FJsonValue>> LocEv;
        LocEv.Add(MakeShared<FJsonValueNumber>(Event.Location.X));
        LocEv.Add(MakeShared<FJsonValueNumber>(Event.Location.Y));
        LocEv.Add(MakeShared<FJsonValueNumber>(Event.Location.Z));
        EvObject->SetArrayField(TEXT("Location"), LocEv);
        EventsArray.Add(MakeShared<FJsonValueObject>(EvObject));
    }
    RootObject->SetArrayField(TEXT("IntentEvents"), EventsArray);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(RootObject, Writer);

    FString FilePath = GetFilePath(PlayerId);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FilePath));
    if (FFileHelper::SaveStringToFile(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("[FileDatabase] Saved player: %s"), *PlayerId);
        Callback(true);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[FileDatabase] Failed to save file: %s"), *FilePath);
        Callback(false);
    }
}

// ============================================================================
// IHktWorldDatabase 구현
// ============================================================================

void UHktFileDatabaseComponent::LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(TUniquePtr<FHktPlayerRecord>)> InCallback)
{
    if (FHktPlayerRecord* Cached = CachedRecords.Find(InPlayerUid))
    {
        InCallback(MakeUnique<FHktPlayerRecord>(*Cached));
        return;
    }

    FString PlayerIdStr = FString::FromInt(InPlayerUid);

    LoadFromFile(PlayerIdStr, [this, InPlayerUid, InCallback](TOptional<FHktPlayerRecord> Loaded)
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
            FHktPlayerRecord NewRecord;
            NewRecord.PlayerUid = InPlayerUid;
            NewRecord.CreatedTime = FDateTime::UtcNow();
            NewRecord.LastLoginTime = NewRecord.CreatedTime;

            CachedRecords.Add(InPlayerUid, NewRecord);
            InCallback(MakeUnique<FHktPlayerRecord>(MoveTemp(NewRecord)));
        }
    });
}

void UHktFileDatabaseComponent::SavePlayerRecordAsync(FHktPlayerRecord InRecord)
{
    int64 Uid = InRecord.PlayerUid;
    CachedRecords.Add(Uid, InRecord);

    FString PlayerIdStr = FString::FromInt(Uid);
    SaveToFile(PlayerIdStr, InRecord, [Uid](bool bSuccess)
    {
        if (!bSuccess)
        {
            UE_LOG(LogTemp, Warning, TEXT("[FileDatabase] Save failed for PlayerUid=%lld"), Uid);
        }
    });
}
