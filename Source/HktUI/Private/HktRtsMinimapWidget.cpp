// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktRtsMinimapWidget.h"
#include "HktUISubsystem.h"
#include "HktUITypes.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "InputCoreTypes.h"

UHktRtsMinimapWidget::UHktRtsMinimapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

FReply UHktRtsMinimapWidget::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseButtonDown(MyGeometry, MouseEvent);

	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) { return FReply::Unhandled(); }

	FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	FVector2D Size = MyGeometry.GetLocalSize();
	if (Size.X <= 0 || Size.Y <= 0) { return FReply::Handled(); }

	float NX = FMath::Clamp(Local.X / Size.X, 0.0f, 1.0f);
	float NY = FMath::Clamp(Local.Y / Size.Y, 0.0f, 1.0f);

	if (MinimapClickEventTag.IsValid())
	{
		if (UHktUISubsystem* Sub = UHktUISubsystem::Get(GetOwningPlayer()))
		{
			FHktUIEvent Event(MinimapClickEventTag);
			Event.Location = FVector(NX, NY, 0.0f);
			Sub->HandleUIEvent(Event);
		}
	}

	return FReply::Handled();
}
