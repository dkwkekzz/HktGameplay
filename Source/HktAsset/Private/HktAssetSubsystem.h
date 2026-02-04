#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Engine/StreamableManager.h"
#include "Engine/DataAsset.h"
#include "HktAssetSubsystem.generated.h"

DECLARE_DELEGATE_OneParam(FOnQueryDataComplete, UDataAsset*);

/**
 * Subsystem to query DataAssets by GameplayTag.
 * Acts as a generic tag-based asset loader.
 */
UCLASS()
class HKTASSET_API UHktAssetSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Queries a single DataAsset associated with the given Tag and loads it asynchronously.
     * @param InTag     The GameplayTag to search for.
     * @param Callback  Function to call when loading is complete. Returns a single UDataAsset*.
     */
    void QueryDataAssetByTag(const FGameplayTag& InTag, FOnQueryDataComplete Callback) const;

private:
    /**
     * Initializes the asset index by scanning the Asset Registry.
     */
    void BuildAssetIndex();

    /**
     * Cached index.
     * Key: GameplayTag
     * Value: Soft Path to the Asset (Unique Mapping)
     */
    TMap<FGameplayTag, FSoftObjectPath> AssetIndex;

    bool bIsIndexBuilt = false;
};
