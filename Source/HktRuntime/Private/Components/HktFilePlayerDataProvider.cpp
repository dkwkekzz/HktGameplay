// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFilePlayerDataProvider.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

FHktFilePlayerDataProvider::FHktFilePlayerDataProvider()
{
}

FString FHktFilePlayerDataProvider::SanitizePlayerIdForPath(const FString& PlayerId)
{
    FString Result = PlayerId;
    const TCHAR InvalidChars[] = TEXT("\\/:*?\"<>|");
    for (const TCHAR* C = InvalidChars; *C; ++C)
    {
        Result.ReplaceCharInline(*C, '_');
    }
    return Result.IsEmpty() ? TEXT("_empty") : Result;
}

FString FHktFilePlayerDataProvider::GetFilePath(const FString& PlayerId) const
{
    return FPaths::ProjectSavedDir() / TEXT("HktPlayerDatabase") / (SanitizePlayerIdForPath(PlayerId) + TEXT(".json"));
}

void FHktFilePlayerDataProvider::Load(const FString& PlayerId, TFunction<void(TOptional<FHktPlayerRecord>)> Callback)
{
    FString FilePath = GetFilePath(PlayerId);
    if (!FPaths::FileExists(FilePath))
    {
        Callback(TOptional<FHktPlayerRecord>());  // 신규 플레이어
        return;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerDataProvider] Failed to load file: %s"), *FilePath);
        return;  // 연결 실패 - 콜백 미호출
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerDataProvider] Failed to parse file: %s"), *FilePath);
        return;
    }

    FHktPlayerRecord Record;
    Record.PlayerId = RootObject->GetStringField(TEXT("PlayerId"));
    Record.ActiveEntityIndex = RootObject->GetIntegerField(TEXT("ActiveEntityIndex"));

    const TArray<TSharedPtr<FJsonValue>>* EntitiesArray;
    if (RootObject->TryGetArrayField(TEXT("Entities"), EntitiesArray))
    {
        for (const TSharedPtr<FJsonValue>& EntityValue : *EntitiesArray)
        {
            const TSharedPtr<FJsonObject>* EntityObject;
            if (!EntityValue->TryGetObject(EntityObject)) continue;

            FHktEntityRecord EntityRecord;
            FGuid::Parse((*EntityObject)->GetStringField(TEXT("PersistentId")), EntityRecord.PersistentId);

            // Properties (JSON Object 또는 Array 호환)
            const TSharedPtr<FJsonObject>* PropsObject;
            const TArray<TSharedPtr<FJsonValue>>* PropsArray;
            if ((*EntityObject)->TryGetObjectField(TEXT("Properties"), PropsObject))
            {
                for (const auto& Prop : (*PropsObject)->Values)
                {
                    int32 PropId = FCString::Atoi(*Prop.Key);
                    EntityRecord.SetProperty(static_cast<uint16>(PropId), static_cast<int32>(Prop.Value->AsNumber()));
                }
            }
            else if ((*EntityObject)->TryGetArrayField(TEXT("Properties"), PropsArray))
            {
                for (int32 i = 0; i < PropsArray->Num(); i++)
                {
                    EntityRecord.SetProperty(static_cast<uint16>(i), static_cast<int32>((*PropsArray)[i]->AsNumber()));
                }
            }

            // Tags
            const TArray<TSharedPtr<FJsonValue>>* TagsArray;
            if ((*EntityObject)->TryGetArrayField(TEXT("Tags"), TagsArray))
            {
                for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
                {
                    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagValue->AsString()), false);
                    if (Tag.IsValid())
                    {
                        EntityRecord.Tags.AddTag(Tag);
                    }
                }
            }

            Record.OwnedEntities.Add(MoveTemp(EntityRecord));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[PlayerDataProvider] Loaded player: %s"), *PlayerId);
    Callback(TOptional<FHktPlayerRecord>(MoveTemp(Record)));
}

void FHktFilePlayerDataProvider::Save(const FString& PlayerId, const FHktPlayerRecord& Record, TFunction<void(bool bSuccess)> Callback)
{
    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetStringField(TEXT("PlayerId"), Record.PlayerId);
    RootObject->SetNumberField(TEXT("ActiveEntityIndex"), Record.ActiveEntityIndex);

    TArray<TSharedPtr<FJsonValue>> EntitiesArray;
    for (const FHktEntityRecord& Entity : Record.OwnedEntities)
    {
        TSharedRef<FJsonObject> EntityObject = MakeShared<FJsonObject>();
        EntityObject->SetStringField(TEXT("PersistentId"), Entity.PersistentId.ToString());

        // Properties (JSON Object: key=PropId, value=number)
        TSharedRef<FJsonObject> PropsObject = MakeShared<FJsonObject>();
        for (int32 PropId = 0; PropId < Entity.Properties.Num(); PropId++)
        {
            int32 Value = Entity.Properties[PropId];
            if (Value != 0)
            {
                PropsObject->SetNumberField(FString::FromInt(PropId), Value);
            }
        }
        EntityObject->SetObjectField(TEXT("Properties"), PropsObject);

        // Tags
        TArray<TSharedPtr<FJsonValue>> TagsArray;
        for (const FGameplayTag& Tag : Entity.Tags)
        {
            TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        }
        EntityObject->SetArrayField(TEXT("Tags"), TagsArray);

        EntitiesArray.Add(MakeShared<FJsonValueObject>(EntityObject));
    }
    RootObject->SetArrayField(TEXT("Entities"), EntitiesArray);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(RootObject, Writer);

    FString FilePath = GetFilePath(PlayerId);
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FilePath));
    if (FFileHelper::SaveStringToFile(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Log, TEXT("[PlayerDataProvider] Saved player: %s"), *PlayerId);
        Callback(true);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[PlayerDataProvider] Failed to save file: %s"), *FilePath);
        Callback(false);
    }
}
