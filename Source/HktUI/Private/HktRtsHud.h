// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HktRtsHud.generated.h"

class UUserWidget;

UCLASS()
class HKTUI_API AHktRtsHud : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RTS|UI")
	TSubclassOf<UUserWidget> MainRtsWidgetClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainRtsWidget;
};
