#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "HktInputAction.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EHktActionTargetType : uint8
{
	None        UMETA(DisplayName = "None"),
	Self        UMETA(DisplayName = "Self"),
	Ally        UMETA(DisplayName = "Ally"),
	Enemy       UMETA(DisplayName = "Enemy"),
	Location    UMETA(DisplayName = "Location"),
};

UCLASS(BlueprintType)
class HKTASSET_API UHktInputAction : public UInputAction
{
	GENERATED_BODY()

public:
	/** Display name of the action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FText ActionName;

	/** Icon for UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UTexture2D> Icon;

	/** The event tag to trigger when executed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	FGameplayTag EventTag;

	/** Targeting requirement for this action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	EHktActionTargetType TargetType = EHktActionTargetType::None;
};
