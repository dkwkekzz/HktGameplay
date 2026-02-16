// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldViewAnchorStrategy.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/LocalPlayerSubsystem.h"

bool UHktWorldViewAnchorStrategy::CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos)
{
	if (!WorldViewPtr || TargetEntityId == InvalidEntityId || !WorldContext)
	{
		return false;
	}

	// WorldView에서 엔티티 위치 읽기 (int32 → float, 1:1 센티미터)
	const int32 PosXInt = WorldViewPtr->GetValue(TargetEntityId, PropertyId::PosX);
	const int32 PosYInt = WorldViewPtr->GetValue(TargetEntityId, PropertyId::PosY);
	const int32 PosZInt = WorldViewPtr->GetValue(TargetEntityId, PropertyId::PosZ);

	// 위치가 모두 0이면 엔티티가 존재하지 않거나 아직 초기화되지 않은 것으로 판단
	if (PosXInt == 0 && PosYInt == 0 && PosZInt == 0)
	{
		return false;
	}

	const FVector WorldLocation = FVector(
		static_cast<float>(PosXInt),
		static_cast<float>(PosYInt),
		static_cast<float>(PosZInt)
	) + WorldOffset;

	// WorldContext에서 World → PlayerController 획득
	UWorld* World = nullptr;
	if (const ULocalPlayerSubsystem* LPS = Cast<ULocalPlayerSubsystem>(WorldContext))
	{
		World = LPS->GetLocalPlayer() ? LPS->GetLocalPlayer()->GetWorld() : nullptr;
	}
	if (!World) return false;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return false;

	return PC->ProjectWorldLocationToScreen(WorldLocation, OutScreenPos);
}
