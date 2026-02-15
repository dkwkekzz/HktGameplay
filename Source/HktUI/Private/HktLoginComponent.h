// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktLoginComponent.generated.h"

/**
 * 로그인 기능을 ActorComponent로 분리. PlayerController에 부착하여 사용합니다.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HKTUI_API UHktLoginComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHktLoginComponent();

	/** [서버] 로그인 요청 수신 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestLogin(const FString& ID, const FString& PW);

	/** [클라이언트] 서버가 보낸 로그인 결과 수신 */
	UFUNCTION(Client, Reliable)
	void Client_ReceiveLoginResult(bool bSuccess, const FString& Token, const FString& InUserID);

	/** 로그인 성공 시 처리 (GameInstance 저장 및 레벨 이동 등) */
	UFUNCTION(BlueprintCallable, Category = "Hkt|Login")
	void OnLoginSuccess(const FString& Token, const FString& InUserID);
};
