// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFilePersistentTickProvider.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

FHktFilePersistentTickProvider::FHktFilePersistentTickProvider()
{
}

FString FHktFilePersistentTickProvider::GetFilePath() const
{
    return FPaths::ProjectSavedDir() / TEXT("HktPersistentTick.json");
}

void FHktFilePersistentTickProvider::ReserveBatch(int64 BatchSize, TFunction<void(int64 NewMaxFrame)> Callback)
{
    // 동기적으로 파일에서 읽고 쓰기 (파일 I/O는 빠르므로 GameThread에서 실행)
    int64 CurrentCounter = 0;

    FString FilePath = GetFilePath();
    if (FPaths::FileExists(FilePath))
    {
        FString JsonString;
        if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
        {
            UE_LOG(LogTemp, Error, TEXT("[PersistentTick] Failed to load file: %s"), *FilePath);
            return;  // 연결 실패 - 콜백 미호출, 서비스 불가
        }

        TSharedPtr<FJsonObject> RootObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
        if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[PersistentTick] Failed to parse file"));
            return;
        }

        CurrentCounter = static_cast<int64>(RootObject->GetNumberField(TEXT("GlobalFrameCounter")));
    }

    int64 NewMaxFrame = CurrentCounter + BatchSize;

    // 파일에 저장
    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetNumberField(TEXT("GlobalFrameCounter"), static_cast<double>(NewMaxFrame));

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (!FJsonSerializer::Serialize(RootObject, Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("[PersistentTick] Failed to serialize"));
        return;
    }

    if (!FFileHelper::SaveStringToFile(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("[PersistentTick] Failed to save file: %s"), *FilePath);
        return;
    }

    // 콜백은 이미 GameThread에서 호출됨
    Callback(NewMaxFrame);
}
