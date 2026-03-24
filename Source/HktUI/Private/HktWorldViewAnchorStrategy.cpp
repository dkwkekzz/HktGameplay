// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldViewAnchorStrategy.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/LocalPlayerSubsystem.h"

bool UHktWorldViewAnchorStrategy::CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos)
{
	if (TargetEntityId == InvalidEntityId || !WorldContext || !bHasWorldPosition)
	{
		return false;
	}

	const FVector WorldLocation = CachedWorldPosition + WorldOffset;

	// WorldContext에서 World → PlayerController 획득
	UWorld* World = nullptr;
	APlayerController* PC = nullptr;
	if (const ULocalPlayerSubsystem* LPS = Cast<ULocalPlayerSubsystem>(WorldContext))
	{
		World = LPS->GetLocalPlayer() ? LPS->GetLocalPlayer()->GetWorld() : nullptr;
	}
	else if (const AHUD* HUD = Cast<AHUD>(WorldContext))
	{
		World = HUD->GetWorld();
		PC = HUD->GetOwningPlayerController();
	}
	if (!World) return false;
	if (!PC) PC = World->GetFirstPlayerController();
	if (!PC) return false;

	return PC->ProjectWorldLocationToScreen(WorldLocation, OutScreenPos);
}
