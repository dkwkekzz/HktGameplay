// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktLoginPlayerController.h"
#include "HktGameInstance.h"
#include "Settings/HktRuntimeGlobalSetting.h"
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
