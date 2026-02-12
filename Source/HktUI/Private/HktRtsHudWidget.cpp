// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktRtsHudWidget.h"
#include "HktUISubsystem.h"
#include "HktUITypes.h"
#include "Components/Button.h"

void UHktRtsHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_CreateUnit) { Button_CreateUnit->OnClicked.AddDynamic(this, &UHktRtsHudWidget::HandleClickCreateUnit); }
}

void UHktRtsHudWidget::HandleClickCreateUnit()
{
	if (!CreateUnitEventTag.IsValid()) { return; }
	if (UHktUISubsystem* Sub = UHktUISubsystem::Get(GetOwningPlayer()))
	{
		FHktUIEvent Event(CreateUnitEventTag);
		Sub->HandleUIEvent(Event);
	}
}
