// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktRtsHud.h"
#include "Blueprint/UserWidget.h"

void AHktRtsHud::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwningPlayerController() || !GetOwningPlayerController()->IsLocalController()) { return; }

	if (MainRtsWidgetClass)
	{
		MainRtsWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), MainRtsWidgetClass);
		if (MainRtsWidget) { MainRtsWidget->AddToViewport(); }
	}
}
