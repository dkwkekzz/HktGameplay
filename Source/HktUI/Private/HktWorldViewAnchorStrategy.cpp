// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldViewAnchorStrategy.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<int32> CVarShowEntityHud(
	TEXT("hkt.Debug.ShowEntityHud"),
	0,
	TEXT("EntityHud 앵커 시각화. 0=끄기, 1=앵커+캡슐 중심"),
	ECVF_Default);

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

	// 디버그 시각화: HUD 앵커 월드 위치 및 캡슐 중심 표시
#if ENABLE_DRAW_DEBUG
	if (CVarShowEntityHud.GetValueOnGameThread() > 0)
	{
		// 머리 위 앵커 포인트 (HUD가 투영되는 위치) — 노란색
		DrawDebugSphere(World, WorldLocation, 8.f, 8, FColor::Yellow, false, -1.f, SDPG_World, 1.5f);
		// 캡슐 중심 (RenderLocation) — 시안색
		DrawDebugSphere(World, CachedRenderLocation, 6.f, 8, FColor::Cyan, false, -1.f, SDPG_World, 1.0f);
		// 캡슐 중심 → 앵커까지 연결선
		DrawDebugLine(World, CachedRenderLocation, WorldLocation, FColor::Yellow, false, -1.f, SDPG_World, 0.8f);
	}
#endif

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

	// ProjectWorldLocationToScreen은 뷰포트 픽셀 좌표를 반환하지만,
	// SConstraintCanvas 슬롯 오프셋은 Slate(DPI 스케일 적용) 좌표를 사용한다.
	// 전체화면 전환 시 DPI 스케일이 변경되어 좌표가 어긋나는 것을 방지하기 위해
	// 뷰포트 픽셀 → Slate 좌표로 변환한다.
	float DPIScale = 1.f;
	if (UGameViewportClient* ViewportClient = World->GetGameViewport())
	{
		DPIScale = ViewportClient->GetDPIScale();
	}
	if (DPIScale <= 0.f) DPIScale = 1.f;

	OutScreenPos /= DPIScale;

	// 스크린 공간 오프셋 적용 (Slate 좌표 기준)
	OutScreenPos += ScreenOffset;

	// 화면 경계 클램핑: 뷰포트 밖으로 나간 좌표를 뷰포트 내로 제한
	int32 ViewportX, ViewportY;
	PC->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0) return false;

	// 뷰포트 크기도 Slate 좌표로 변환
	const float VX = static_cast<float>(ViewportX) / DPIScale;
	const float VY = static_cast<float>(ViewportY) / DPIScale;
	OutScreenPos.X = FMath::Clamp(OutScreenPos.X, 0.f, VX);
	OutScreenPos.Y = FMath::Clamp(OutScreenPos.Y, 0.f, VY);

	return true;
}
