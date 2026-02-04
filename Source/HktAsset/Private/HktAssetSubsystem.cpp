#include "HktAssetSubsystem.h"
#include "HktTagDataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"

void UHktAssetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RebuildTagMap();
}

void UHktAssetSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

UHktAssetSubsystem* UHktAssetSubsystem::Get(UWorld* World)
{
    if (World && World->GetGameInstance())
    {
        return World->GetGameInstance()->GetSubsystem<UHktAssetSubsystem>();
    }
    return nullptr;
}

void UHktAssetSubsystem::RebuildTagMap()
{
    TagToPathMap.Empty();

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.ClassPaths.Add(UHktTagDataAsset::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    for (const FAssetData& AssetData : AssetList)
    {
        FString TagString;
        if (AssetData.GetTagValue(FName("IdentifierTag"), TagString))
        {
            FGameplayTag FoundTag = FGameplayTag::RequestGameplayTag(FName(*TagString));
            if (FoundTag.IsValid())
            {
                // SoftObjectPath만 저장하여 메모리 절약
                TagToPathMap.Add(FoundTag, AssetData.ToSoftObjectPath());
            }
        }
    }
}

UHktTagDataAsset* UHktAssetSubsystem::LoadAssetSync(FGameplayTag Tag)
{
    if (!Tag.IsValid()) return nullptr;

    if (const FSoftObjectPath* Path = TagToPathMap.Find(Tag))
    {
        if (Path->IsValid())
        {
            // ResolveObject는 이미 메모리에 있는 경우 빠르게 가져옵니다.
            if (UObject* ResolvedObj = Path->ResolveObject())
            {
                return Cast<UHktTagDataAsset>(ResolvedObj);
            }
            // 메모리에 없다면 동기 로드 (프레임 드랍 주의)
            return Cast<UHktTagDataAsset>(Path->TryLoad());
        }
    }
    return nullptr;
}

void UHktAssetSubsystem::LoadAssetAsync(FGameplayTag Tag, FStreamableDelegate DelegateToCall)
{
    if (!Tag.IsValid())
    {
        DelegateToCall.ExecuteIfBound();
        return;
    }

    if (const FSoftObjectPath* Path = TagToPathMap.Find(Tag))
    {
        if (Path->IsValid())
        {
            StreamableManager.RequestAsyncLoad(*Path, DelegateToCall);
            return;
        }
    }
    
    // 실패 시에도 델리게이트는 실행해주는 것이 안전할 수 있음 (상황에 따라 다름)
    UE_LOG(LogTemp, Warning, TEXT("[HktAssetSubsystem] Async Load Failed: Tag not found %s"), *Tag.ToString());
}

void UHktAssetSubsystem::LoadAssetAsync(FGameplayTag Tag, TFunction<void(UHktTagDataAsset*)> OnLoaded)
{
    // C++ 람다 편의 함수 구현
    // FStreamableDelegate를 생성하여 내부적으로 연결합니다.
    FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &UHktAssetSubsystem::OnAssetLoadedInternal, Tag, OnLoaded);
    LoadAssetAsync(Tag, Delegate);
}

void UHktAssetSubsystem::OnAssetLoadedInternal(FGameplayTag Tag, TFunction<void(UHktTagDataAsset*)> Callback)
{
    // 로딩이 완료된 시점에 호출됩니다.
    // 이때는 이미 메모리에 올라와 있으므로 LoadAssetSync가 즉시 리턴됩니다.
    UHktTagDataAsset* LoadedAsset = LoadAssetSync(Tag);
    
    if (Callback)
    {
        Callback(LoadedAsset);
    }
}