// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "HktRtsHudWidget.generated.h"

class UButton;

UCLASS()
class HKTUI_API UHktRtsHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleClickCreateUnit();

	UPROPERTY(EditDefaultsOnly, Category = "RTS|HUD")
	FGameplayTag CreateUnitEventTag;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_CreateUnit;
};
