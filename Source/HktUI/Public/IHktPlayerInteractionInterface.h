// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "HktRuntimeTypes.h"
#include "IHktPlayerInteractionInterface.generated.h"

/**
 * UI가 PlayerController에게 이벤트를 전달하기 위한 통신 인터페이스.
 * PlayerController에서 구현하여 로그인/시뮬레이션 등으로 라우팅합니다.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UHktPlayerInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

class HKTUI_API IHktPlayerInteractionInterface
{
	GENERATED_BODY()

public:
	/** 일반적인 게임플레이 관련 명령 전달 (Component로 라우팅) */
	virtual void HandleUICommand(FGameplayTag CommandTag, const FString& Payload) = 0;

	/** 시뮬레이션 시스템으로 이벤트 전달 */
	virtual void SendRuntimeEvent(const FHktRuntimeEvent& Event) = 0;
};
