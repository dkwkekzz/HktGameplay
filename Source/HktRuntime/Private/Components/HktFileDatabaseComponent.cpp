// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFileDatabaseComponent.h"
#include "HktRuntimeConverter.h"
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
// 파일 로드 (FHktPlayerRecord: PlayerUid, EntityStates, IntentEvents, ...)
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
    if (RootObject->TryGetArrayField(TEXT("EntityStates"), EntitiesArray))
    {
        for (const TSharedPtr<FJsonValue>& EntityValue : *EntitiesArray)
        {
            const TSharedPtr<FJsonObject>* EntityObject;
            if (!EntityValue->TryGetObject(EntityObject)) continue;

            // Core 구조체로 생성
            FHktEntityState CoreState;
            CoreState.EntityId = static_cast<int32>((*EntityObject)->GetNumberField(TEXT("EntityId")));

            const TArray<TSharedPtr<FJsonValue>>* PropsArray;
            if ((*EntityObject)->TryGetArrayField(TEXT("Properties"), PropsArray))
            {
                for (const TSharedPtr<FJsonValue>& P : *PropsArray)
                {
                    CoreState.Properties.Add(static_cast<int32>(P->AsNumber()));
                }
            }

            // TODO: 런타임에 유효한 값이라 이렇게 하면 안됨...
            const TArray<TSharedPtr<FJsonValue>>* TagsArray;
            if ((*EntityObject)->TryGetArrayField(TEXT("Tags"), TagsArray))
            {
                for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
                {
                    CoreState.TagIndices.Add(static_cast<int32>(TagValue->AsNumber()));
                }
            }

            Record.EntityStates.Add(CoreState);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* EventsArray;
    if (RootObject->TryGetArrayField(TEXT("Events"), EventsArray))
    {
        for (const TSharedPtr<FJsonValue>& EvValue : *EventsArray)
        {
            const TSharedPtr<FJsonObject>* EvObject;
            if (!EvValue->TryGetObject(EvObject)) continue;
            
            // Core 구조체로 생성
            FHktEvent CoreEvent;
            CoreEvent.EventId = static_cast<int32>((*EvObject)->GetNumberField(TEXT("EventId")));
            CoreEvent.SourceEntity = static_cast<int32>((*EvObject)->GetNumberField(TEXT("SourceEntityId")));
            CoreEvent.TargetEntity = static_cast<int32>((*EvObject)->GetNumberField(TEXT("TargetEntityId")));
            CoreEvent.EventTag = FGameplayTag::RequestGameplayTag(FName(*(*EvObject)->GetStringField(TEXT("EventTag"))), false);
            const TArray<TSharedPtr<FJsonValue>>* LocEv;
            if ((*EvObject)->TryGetArrayField(TEXT("Location"), LocEv) && LocEv->Num() >= 3)
            {
                CoreEvent.Location.X = (*LocEv)[0]->AsNumber();
                CoreEvent.Location.Y = (*LocEv)[1]->AsNumber();
                CoreEvent.Location.Z = (*LocEv)[2]->AsNumber();
            }
            CoreEvent.Param0 = 0;
            CoreEvent.Param1 = 0;
            
            // Zero-Cost 변환: reinterpret_cast를 사용하여 RuntimeEvent로 변환
            Record.Events.Add(CoreEvent);
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
    for (const FHktEntityState& CoreState : Record.EntityStates)
    {
        TSharedRef<FJsonObject> EntityObject = MakeShared<FJsonObject>();
        EntityObject->SetNumberField(TEXT("EntityId"), CoreState.EntityId);
        TArray<TSharedPtr<FJsonValue>> PropsArray;
        for (int32 V : CoreState.Properties)
        {
            PropsArray.Add(MakeShared<FJsonValueNumber>(V));
        }
        EntityObject->SetArrayField(TEXT("Properties"), PropsArray);
        TArray<TSharedPtr<FJsonValue>> TagsArray;
        for (int32 TagIndex : CoreState.TagIndices)
        {
            TagsArray.Add(MakeShared<FJsonValueNumber>(TagIndex));
        }
        EntityObject->SetArrayField(TEXT("Tags"), TagsArray);
        EntitiesArray.Add(MakeShared<FJsonValueObject>(EntityObject));
    }
    RootObject->SetArrayField(TEXT("EntitySnapshots"), EntitiesArray);

    TArray<TSharedPtr<FJsonValue>> EventsArray;
    for (const FHktEvent& CoreEvent : Record.Events)
    {
        TSharedRef<FJsonObject> EvObject = MakeShared<FJsonObject>();
        EvObject->SetNumberField(TEXT("EventId"), CoreEvent.EventId);
        EvObject->SetNumberField(TEXT("SourceEntityId"), CoreEvent.SourceEntity);
        EvObject->SetNumberField(TEXT("TargetEntityId"), CoreEvent.TargetEntity);
        EvObject->SetStringField(TEXT("EventTag"), CoreEvent.EventTag.ToString());
        TArray<TSharedPtr<FJsonValue>> LocEv;
        LocEv.Add(MakeShared<FJsonValueNumber>(CoreEvent.Location.X));
        LocEv.Add(MakeShared<FJsonValueNumber>(CoreEvent.Location.Y));
        LocEv.Add(MakeShared<FJsonValueNumber>(CoreEvent.Location.Z));
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
