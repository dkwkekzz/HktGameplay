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

bool AHktLoginPlayerController::Server_RequestLogin_Validate(const FString& ID, const FString& PW)
{
	return !ID.IsEmpty();
}

void AHktLoginPlayerController::Server_RequestLogin_Implementation(const FString& ID, const FString& PW)
{
	// 서버에서 로그인 검증 (TODO: 실제 웹 API/DB 연동)
	// 현재는 목(mock): 유효한 ID면 성공
	const bool bSuccess = !ID.IsEmpty();
	const FString Token = bSuccess ? (TEXT("MockToken_") + ID) : FString();
	const FString UserID = bSuccess ? ID : FString();

	Client_ReceiveLoginResult(bSuccess, Token, UserID);
}

void AHktLoginPlayerController::Client_ReceiveLoginResult_Implementation(bool bSuccess, const FString& Token, const FString& InUserID)
{
	if (bSuccess)
	{
		OnLoginSuccess(Token, InUserID);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HktLoginPlayerController: Login failed (server rejected)"));
	}
}

void AHktLoginPlayerController::OnLoginSuccess(const FString& Token, const FString& InUserID)
{
	UHktGameInstance* GI = Cast<UHktGameInstance>(GetGameInstance());
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("HktLoginPlayerController: GameInstance is not UHktGameInstance"));
		return;
	}

	GI->UserSessionToken = Token;
	GI->UserID = InUserID;

	const UHktRuntimeGlobalSetting* Settings = GetDefault<UHktRuntimeGlobalSetting>();
	if (!Settings || Settings->InGameMap.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("HktLoginPlayerController: InGameMap is not set in Hkt Runtime Settings"));
		return;
	}

	const TSoftObjectPtr<UWorld>& Level = Settings->InGameMap;
	UE_LOG(LogTemp, Log, TEXT("HktLoginPlayerController: Login success, opening level '%s'"), *Level.GetLongPackageName());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level);
}

void AHktLoginPlayerController::OnUserEvent(const FHktUserEvent& Event)
{
	if (Event.Datas.Num() < 2)
		return;

	const FString ID = Event.Datas[0].GetValue<FString>();
	const FString PW = Event.Datas[1].GetValue<FString>();
	
	// 클라이언트에서 호출 → 서버로 RPC
	Server_RequestLogin(ID, PW);

	UE_LOG(LogTemp, Log, TEXT("HktLoginPlayerController: OnUserEvent: %s"), *Event.Name.ToString());
}