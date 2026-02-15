// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUIElement.h"
#include "IHktUIView.h"
#include "HktUIAnchorStrategy.h"
#include "Subsystems/WorldSubsystem.h"

void UHktUIElement::InitializeElement(TSharedPtr<IHktUIView> InView, UHktUIAnchorStrategy* InAnchorStrategy)
{
	View = MoveTemp(InView);
	AnchorStrategy = InAnchorStrategy;
	CachedScreenPosition = FVector2D::ZeroVector;
}

void UHktUIElement::TickElement(float DeltaTime)
{
	if (!View.IsValid() || !AnchorStrategy) return;

	// WorldContext: 이 Element를 소유한 Subsystem을 넘기려면 Outer가 UWorldSubsystem이어야 함.
	// 호출측(UHktUISubsystem::Tick)에서 this(Subsystem)를 넘기므로, 여기서는 Outer 사용.
	UObject* WorldContext = GetOuter();
	if (!WorldContext) return;

	FVector2D ScreenPos;
	if (AnchorStrategy->CalculateScreenPosition(WorldContext, ScreenPos))
	{
		CachedScreenPosition = ScreenPos;
	}
}

void UHktUIElement::SetParent(UHktUIElement* InParent)
{
	if (Parent == InParent) return;
	if (Parent)
	{
		Parent->Children.Remove(this);
	}
	Parent = InParent;
	if (Parent)
	{
		Parent->Children.AddUnique(this);
	}
}
