// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUIActorAnchorStrategy.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"

bool UHktUIActorAnchorStrategy::CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos)
{
	if (!TargetActor.IsValid() || !WorldContext) return false;

	UWorld* World = nullptr;
	APlayerController* PC = nullptr;
	if (const UWorldSubsystem* WS = Cast<UWorldSubsystem>(WorldContext))
	{
		World = WS->GetWorld();
	}
	else if (const ULocalPlayerSubsystem* LPS = Cast<ULocalPlayerSubsystem>(WorldContext))
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

	FVector TargetLoc = TargetActor->GetActorLocation() + WorldOffset;
	return PC->ProjectWorldLocationToScreen(TargetLoc, OutScreenPos);
}
