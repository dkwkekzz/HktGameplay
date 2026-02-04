// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "HktIntentEventDataAsset.generated.h"

/**
 * Asset that defines how an Intent Event maps to Effects.
 * Each asset handles one EventTag and specifies what effects to apply to Subjects and Targets.
 */
UCLASS(BlueprintType)
class HKTASSET_API UHktIntentEventDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The EventTag this asset handles */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mapping")
	FGameplayTag EventTag;
};
