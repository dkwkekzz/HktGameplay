#include "HktAssetSubsystem.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

void UHktAssetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 인덱스 구축
    BuildAssetIndex();
}

void UHktAssetSubsystem::Deinitialize()
{
    AssetIndex.Empty();
    bIsIndexBuilt = false;
    Super::Deinitialize();
}

void UHktAssetSubsystem::QueryDataAssetByTag(const FGameplayTag& InTag, FOnQueryDataComplete Callback) const
{
    // 1. 유효하지 않은 태그 처리
    if (!InTag.IsValid())
    {
        Callback.ExecuteIfBound(nullptr);
        return;
    }

    // 2. 인덱스 검색 (TMap Find)
    const FSoftObjectPath* FoundPath = AssetIndex.Find(InTag);
    if (!FoundPath)
    {
        // 태그에 매핑된 에셋이 없음
        Callback.ExecuteIfBound(nullptr);
        return;
    }

    // 3. 비동기 로딩 요청 (단일 에셋)
    FStreamableManager& Streamable = UAssetManager::Get().GetStreamableManager();
    
    // FSoftObjectPath는 포인터가 아닌 값으로 복사하여 람다에 전달
    FSoftObjectPath PathToLoad = *FoundPath;

    Streamable.RequestAsyncLoad(
        PathToLoad,
        FStreamableDelegate::CreateLambda([Callback, PathToLoad]()
        {
            UDataAsset* LoadedAsset = nullptr;
            
            if (UObject* ResolvedObj = PathToLoad.ResolveObject())
            {
                LoadedAsset = Cast<UDataAsset>(ResolvedObj);
            }

            Callback.ExecuteIfBound(LoadedAsset);
        })
    );
}

void UHktAssetSubsystem::BuildAssetIndex()
{
    if (bIsIndexBuilt)
    {
        return;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> AssetDataList;
    
    // 특정 파생 클래스가 아닌, 모든 UDataAsset 기반 에셋을 대상으로 검색합니다.
    // 이렇게 하면 HktActionDataAsset 뿐만 아니라 다른 타입의 DataAsset도 태그만 있다면 자동으로 인덱싱됩니다.
    FTopLevelAssetPath ClassPath = UDataAsset::StaticClass()->GetClassPathName();
    AssetRegistry.GetAssetsByClass(ClassPath, AssetDataList, true);

    const FName TagName = TEXT("ActionTag");

    for (const FAssetData& Data : AssetDataList)
    {
        // 에셋 레지스트리에 'ActionTag' 메타데이터가 있는지 확인
        if (Data.TagsAndValues.Contains(TagName))
        {
            FAssetTagValueRef ValueRef = Data.TagsAndValues.FindTag(TagName);
            FGameplayTag ActionTag = FGameplayTag::RequestGameplayTag(ValueRef.AsName());

            if (ActionTag.IsValid())
            {
                // 중복 태그 감지 및 처리
                if (AssetIndex.Contains(ActionTag))
                {
                    // 어떤 에셋이 덮어쓰여지는지 로그로 명확히 남겨 디버깅을 돕습니다.
                    UE_LOG(LogTemp, Warning, TEXT("UHktAssetSubsystem: Duplicate Tag [%s] detected. Asset [%s] will overwrite existing entry."), 
                        *ActionTag.ToString(), 
                        *Data.AssetName.ToString());
                }
                AssetIndex.Add(ActionTag, Data.ToSoftObjectPath());
            }
        }
    }

    bIsIndexBuilt = true;
}
