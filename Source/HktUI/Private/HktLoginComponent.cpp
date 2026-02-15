// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktLoginComponent.h"

UHktLoginComponent::UHktLoginComponent()
{
	SetIsReplicatedByDefault(true);
}

bool UHktLoginComponent::Server_RequestLogin_Validate(const FString& ID, const FString& PW)
{
	return !ID.IsEmpty() && !PW.IsEmpty();
}

void UHktLoginComponent::Server_RequestLogin_Implementation(const FString& ID, const FString& PW)
{
	// TODO: 실제 서버 로직 (DB 검증, 토큰 발급 등)
	// 임시: 클라이언트에 성공 결과 전달
	Client_ReceiveLoginResult(true, TEXT("TempToken"), ID);
}

void UHktLoginComponent::Client_ReceiveLoginResult_Implementation(bool bSuccess, const FString& Token, const FString& InUserID)
{
	if (bSuccess)
	{
		OnLoginSuccess(Token, InUserID);
	}
}

void UHktLoginComponent::OnLoginSuccess(const FString& Token, const FString& InUserID)
{
	// TODO: GameInstance에 토큰/유저 ID 저장, 레벨 이동 등
}
