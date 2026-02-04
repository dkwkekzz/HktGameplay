// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HktUserEventConsumer.h"
#include "HktEntryPlayerController.generated.h"

/**
 * 로그인 맵 전용 PlayerController.
 * UI 입력만 처리하며, RequestLogin 성공 시 GameInstance에 토큰 저장 후 인게임 맵으로 전환.
 */
UCLASS()
class HKTRUNTIME_API AHktEntryPlayerController : public APlayerController, public IHktUserEventConsumer
{
	GENERATED_BODY()

public:
	AHktEntryPlayerController();

	/** 로그인 요청 (ID/PW). 성공 시 OnLoginSuccess 호출 후 인게임 맵으로 이동 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|Login")
	void RequestLogin(const FString& ID, const FString& PW);

	/** 로그인 성공 시 호출. 토큰·UserID를 GameInstance에 저장하고 InGame 맵으로 OpenLevel */
	UFUNCTION(BlueprintCallable, Category = "Hkt|Login")
	void OnLoginSuccess(const FString& Token, const FString& InUserID);

protected:
	virtual void BeginPlay() override;

	// === IHktUserEventConsumer Interface ===
	virtual void OnUserEvent(const FHktUserEvent& Event) override;
};
