// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Slates/SHktLoginHudWidget.h"
#include "HktLoginHud.generated.h"

class UHktWidgetLoginHudDataAsset;
class UHktUISubsystem;
class UHktEventParam;

UCLASS()
class HKTUI_API AHktLoginHud : public AHUD
{
	GENERATED_BODY()

public:
	AHktLoginHud();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void AddLoginWidgetToViewport();
	void CreateAndAddLoginWidget(const TOptional<struct FSlateBrush>& BackgroundBrush, UHktWidgetLoginHudDataAsset* DataAsset);
	void RemoveLoginWidgetFromViewport();

	void OnLoginRequested(const FString& ID, const FString& PW);

	UFUNCTION()
	void OnLoginCompleted(UHktEventParam* Param, bool bSuccess);

	TSharedPtr<class SWidget> LoginWidgetSlate;
	TWeakObjectPtr<UHktUISubsystem> UISubsystem;
};
