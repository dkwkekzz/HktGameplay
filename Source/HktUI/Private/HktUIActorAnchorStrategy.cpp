// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUIActorAnchorStrategy.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"

bool UHktUIActorAnchorStrategy::CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos)
{
	if (!TargetActor.IsValid() || !WorldContext) return false;

	UWorld* World = nullptr;
	if (const UWorldSubsystem* WS = Cast<UWorldSubsystem>(WorldContext))
	{
		World = WS->GetWorld();
	}
	else if (const ULocalPlayerSubsystem* LPS = Cast<ULocalPlayerSubsystem>(WorldContext))
	{
		World = LPS->GetLocalPlayer() ? LPS->GetLocalPlayer()->GetWorld() : nullptr;
	}
	if (!World) return false;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return false;

	FVector TargetLoc = TargetActor->GetActorLocation() + WorldOffset;
	return PC->ProjectWorldLocationToScreen(TargetLoc, OutScreenPos);
}
