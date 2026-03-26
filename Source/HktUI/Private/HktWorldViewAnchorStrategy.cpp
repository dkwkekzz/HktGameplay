// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldViewAnchorStrategy.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/LocalPlayerSubsystem.h"

FVector UHktWorldViewAnchorStrategy::GetHeadWorldLocation() const
{
	// RenderLocation은 이미 지면 트레이스 + 캡슐 오프셋이 적용된 최종 렌더 위치 (캡슐 중심).
	// 캡슐 상단(머리) = RenderLocation + CapsuleHalfHeight, 거기에 약간의 여백(HeadClearance)을 추가.
	return CachedRenderLocation + FVector(0.f, 0.f, CapsuleHalfHeight + HeadClearance);
}

bool UHktWorldViewAnchorStrategy::CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos)
{
	if (TargetEntityId == InvalidEntityId || !WorldContext || !bHasWorldPosition)
	{
		return false;
	}

	const FVector WorldLocation = GetHeadWorldLocation();

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

	// 카메라 뒤에 있는 엔티티 필터링: ProjectWorldLocationToScreen은 카메라 뒤 좌표도
	// 화면에 투영하여 잘못된 위치를 반환할 수 있음.
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector ToEntity = WorldLocation - CamLoc;
	if (FVector::DotProduct(ToEntity, CamRot.Vector()) <= 0.f)
	{
		return false;
	}

	if (!PC->ProjectWorldLocationToScreen(WorldLocation, OutScreenPos))
	{
		return false;
	}

	// ProjectWorldLocationToScreen은 스크린 픽셀 좌표를 반환하지만
	// SConstraintCanvas는 Slate 단위(DPI 독립)로 동작하므로 DPI 스케일로 나눠야 함.
	if (UGameViewportClient* ViewportClient = World->GetGameViewport())
	{
		const float DPIScale = ViewportClient->GetDPIScale();
		if (DPIScale > 0.f && DPIScale != 1.f)
		{
			OutScreenPos /= DPIScale;
		}
	}

	// 화면 경계 클램핑: 뷰포트 밖으로 나간 좌표를 뷰포트 내로 제한 (Slate 단위)
	int32 ViewportX, ViewportY;
	PC->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0) return false;

	// GetViewportSize도 픽셀 단위이므로 동일하게 DPI 변환
	float VX = static_cast<float>(ViewportX);
	float VY = static_cast<float>(ViewportY);
	if (UGameViewportClient* ViewportClient = World->GetGameViewport())
	{
		const float DPIScale = ViewportClient->GetDPIScale();
		if (DPIScale > 0.f && DPIScale != 1.f)
		{
			VX /= DPIScale;
			VY /= DPIScale;
		}
	}
	OutScreenPos.X = FMath::Clamp(OutScreenPos.X, 0.f, VX);
	OutScreenPos.Y = FMath::Clamp(OutScreenPos.Y, 0.f, VY);

	return true;
}
