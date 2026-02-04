// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktEntryPlayerController.h"
#include "HktGameInstance.h"
#include "Settings/HktRuntimeGlobalSetting.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/InputSettings.h"

AHktEntryPlayerController::AHktEntryPlayerController()
{
}

void AHktEntryPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 로그인 화면은 UI 전용 입력
	SetInputMode(FInputModeUIOnly());
	bShowMouseCursor = true;
}

void AHktEntryPlayerController::RequestLogin(const FString& ID, const FString& PW)
{
	// TODO: 실제 웹 API 호출 (HTTP 등). 성공 시 OnLoginSuccess(Token, UserID) 호출
	// 현재는 목(mock) 구현: 바로 성공으로 처리
	OnLoginSuccess(TEXT("MockToken_") + ID, ID);
}

void AHktEntryPlayerController::OnLoginSuccess(const FString& Token, const FString& InUserID)
{
	UHktGameInstance* GI = Cast<UHktGameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("HktEntryPlayerController: GameInstance is not UHktGameInstance"));
		return;
	}

	GI->UserSessionToken = Token;
	GI->UserID = InUserID;

	const UHktRuntimeGlobalSetting* Settings = GetDefault<UHktRuntimeGlobalSetting>();
	if (!Settings || Settings->InGameMap.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("HktEntryPlayerController: InGameMap is not set in Hkt Runtime Settings"));
		return;
	}

	const TSoftObjectPtr<UWorld>& Level = Settings->InGameMap;
	UE_LOG(LogTemp, Log, TEXT("HktEntryPlayerController: Login success, opening level '%s'"), *Level.GetLongPackageName());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level);
}

void AHktEntryPlayerController::OnUserEvent(const FHktUserEvent& Event)
{
	if (Event.Datas.Num() < 2)
		return;

	const FString ID = Event.Datas[0].GetValue<FString>();
	const FString PW = Event.Datas[1].GetValue<FString>();
	RequestLogin(ID, PW);

	UE_LOG(LogTemp, Log, TEXT("HktEntryPlayerController: OnUserEvent: %s"), *Event.Name.ToString());
}