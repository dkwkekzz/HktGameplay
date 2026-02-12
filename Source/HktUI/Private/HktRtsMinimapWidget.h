// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "HktRtsMinimapWidget.generated.h"

UCLASS()
class HKTUI_API UHktRtsMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHktRtsMinimapWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, Category = "RTS|Minimap")
	FGameplayTag MinimapClickEventTag;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
};
