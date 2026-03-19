#include "HktAssetSubsystem.h"
#include "HktTagDataAsset.h"
#include "HktAssetSettings.h"
#include "HktCoreEventLog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktAssetSubsystem, Log, All);

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

    UE_LOG(LogHktAssetSubsystem, Log, TEXT("RebuildTagMap: %d tags registered"), TagToPathMap.Num());
    HKT_EVENT_LOG("Asset", FString::Printf(TEXT("RebuildTagMap complete")));
}

// ============================================================================
// 경로 해결 (TagMap → Convention → OnMiss 순서)
// ============================================================================

FSoftObjectPath UHktAssetSubsystem::ResolvePath(FGameplayTag Tag)
{
    HKT_EVENT_LOG("Asset", FString::Printf(TEXT("ResolvePath: %s"), *Tag.ToString()));

    // 1. TagMap (DataAsset 기반)
    if (const FSoftObjectPath* Path = TagToPathMap.Find(Tag))
    {
        if (Path->IsValid())
        {
            return *Path;
        }
    }

    // 2. Convention Path (규칙 기반)
    FSoftObjectPath ConventionPath = ResolveConventionPath(Tag);
    if (ConventionPath.IsValid())
    {
        // Convention Path에 에셋이 실제로 존재하는지 확인
        FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FAssetData AssetData = ARM.Get().GetAssetByObjectPath(ConventionPath);
        if (AssetData.IsValid())
        {
            // 발견 시 캐시에 등록
            TagToPathMap.Add(Tag, ConventionPath);
            UE_LOG(LogHktAssetSubsystem, Log, TEXT("Convention hit: %s → %s"), *Tag.ToString(), *ConventionPath.ToString());
            return ConventionPath;
        }
    }

    // 3. OnMiss 콜백 (Generator 연동)
    if (OnTagMiss.IsBound())
    {
        FSoftObjectPath GeneratedPath = OnTagMiss.Execute(Tag);
        if (GeneratedPath.IsValid())
        {
            TagToPathMap.Add(Tag, GeneratedPath);
            UE_LOG(LogHktAssetSubsystem, Log, TEXT("OnTagMiss resolved: %s → %s"), *Tag.ToString(), *GeneratedPath.ToString());
            return GeneratedPath;
        }
    }

    return FSoftObjectPath();
}

// ============================================================================
// Convention Path Resolution
// ============================================================================

FSoftObjectPath UHktAssetSubsystem::ResolveConventionPath(const FGameplayTag& Tag)
{
    if (!Tag.IsValid()) return FSoftObjectPath();

    const UHktAssetSettings* Settings = UHktAssetSettings::Get();
    if (!Settings) return FSoftObjectPath();

    FString ResolvedPath = Settings->ResolveConventionPath(Tag.ToString());
    if (ResolvedPath.IsEmpty()) return FSoftObjectPath();

    // AssetName 추출 (마지막 / 이후)
    FString AssetName;
    int32 LastSlash;
    if (ResolvedPath.FindLastChar('/', LastSlash))
    {
        AssetName = ResolvedPath.Mid(LastSlash + 1);
    }
    else
    {
        AssetName = ResolvedPath;
    }

    // FSoftObjectPath 형식: PackagePath.AssetName
    return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *ResolvedPath, *AssetName));
}

UObject* UHktAssetSubsystem::LoadByConventionSync(FGameplayTag Tag)
{
    FSoftObjectPath Path = ResolvePath(Tag);
    if (!Path.IsValid()) return nullptr;

    if (UObject* Resolved = Path.ResolveObject())
    {
        return Resolved;
    }
    return Path.TryLoad();
}

// ============================================================================
// 등록 / 재구축
// ============================================================================

void UHktAssetSubsystem::RegisterTagPath(FGameplayTag Tag, FSoftObjectPath Path)
{
    if (Tag.IsValid() && Path.IsValid())
    {
        TagToPathMap.Add(Tag, Path);
        UE_LOG(LogHktAssetSubsystem, Log, TEXT("RegisterTagPath: %s → %s"), *Tag.ToString(), *Path.ToString());
    }
}

void UHktAssetSubsystem::ForceRebuildTagMap()
{
    RebuildTagMap();
}

// ============================================================================
// 로드 API (기존 — ResolvePath 활용으로 확장)
// ============================================================================

UHktTagDataAsset* UHktAssetSubsystem::LoadAssetSync(FGameplayTag Tag)
{
    if (!Tag.IsValid()) return nullptr;

    FSoftObjectPath Path = ResolvePath(Tag);
    if (!Path.IsValid())
    {
        UE_LOG(LogHktAssetSubsystem, Warning, TEXT("LoadAssetSync: Tag not resolved: %s"), *Tag.ToString());
        return nullptr;
    }

    if (UObject* ResolvedObj = Path.ResolveObject())
    {
        return Cast<UHktTagDataAsset>(ResolvedObj);
    }
    return Cast<UHktTagDataAsset>(Path.TryLoad());
}

void UHktAssetSubsystem::LoadAssetAsync(FGameplayTag Tag, FStreamableDelegate DelegateToCall)
{
    if (!Tag.IsValid())
    {
        DelegateToCall.ExecuteIfBound();
        return;
    }

    FSoftObjectPath Path = ResolvePath(Tag);
    if (Path.IsValid())
    {
        StreamableManager.RequestAsyncLoad(Path, DelegateToCall);
        return;
    }

    UE_LOG(LogHktAssetSubsystem, Warning, TEXT("LoadAssetAsync: Tag not resolved: %s"), *Tag.ToString());
}

void UHktAssetSubsystem::LoadAssetAsync(FGameplayTag Tag, TFunction<void(UHktTagDataAsset*)> OnLoaded)
{
    FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &UHktAssetSubsystem::OnAssetLoadedInternal, Tag, OnLoaded);
    LoadAssetAsync(Tag, Delegate);
}

void UHktAssetSubsystem::OnAssetLoadedInternal(FGameplayTag Tag, TFunction<void(UHktTagDataAsset*)> Callback)
{
    UHktTagDataAsset* LoadedAsset = LoadAssetSync(Tag);

    if (Callback)
    {
        Callback(LoadedAsset);
    }
}
