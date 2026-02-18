// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "HktRuntimeTypes.h"
#include "HktRuntimeDelegates.h"
#include "IHktPlayerInteractionInterface.generated.h"

/**
 * UI가 PlayerController에게 이벤트를 전달하고, WorldView를 조회하기 위한 통신 인터페이스.
 * PlayerController에서 구현하여 로그인/시뮬레이션 등으로 라우팅합니다.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UHktPlayerInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktPlayerInteractionInterface
{
	GENERATED_BODY()

public:
	/** 일반적인 게임플레이 관련 명령 전달 (UObject를 통한 유연한 데이터 전달) */
	virtual void ExecuteCommand(UObject* CommandData) = 0;

	/** 현재 클라이언트의 WorldView를 가져옵니다. 시뮬레이터 미초기화 시 false 반환. */
	virtual bool GetWorldView(FHktWorldView& OutView) const = 0;

	/** WorldView가 갱신되었을 때 (FrameBatch/InitialState 수신 후) 브로드캐스트됩니다. */
	virtual FOnHktWorldViewUpdated& OnWorldViewUpdated() = 0;
};
