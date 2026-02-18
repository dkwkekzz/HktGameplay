// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktLoginPlayerController.h"
#include "HktGameInstance.h"
#include "HktRuntimeCommands.h"
#include "Settings/HktRuntimeGlobalSetting.h"
#include "Components/HktLoginComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/InputSettings.h"

AHktLoginPlayerController::AHktLoginPlayerController()
{
}

void AHktLoginPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로그인 화면은 UI 전용 입력
	SetInputMode(FInputModeUIOnly());
	bShowMouseCursor = true;
}

// ============================================================================
// IHktPlayerInteractionInterface 구현
// ============================================================================

void AHktLoginPlayerController::ExecuteCommand(UObject* CommandData)
{
	if (!CommandData)
	{
		return;
	}

	// 로그인 명령 처리
	if (UHktLoginRequest* LoginRequest = Cast<UHktLoginRequest>(CommandData))
	{
		// HktLoginComponent 찾기
		UHktLoginComponent* LoginComp = FindComponentByClass<UHktLoginComponent>();
		if (LoginComp)
		{
			LoginComp->Server_RequestLogin(LoginRequest->UserID, LoginRequest->Password);
		}
	}
}

bool AHktLoginPlayerController::GetWorldView(FHktWorldView& OutView) const
{
	// 로그인 화면에서는 사용하지 않음
	return false;
}

FOnHktWorldViewUpdated& AHktLoginPlayerController::OnWorldViewUpdated()
{
	return WorldViewUpdatedDelegate;
}
