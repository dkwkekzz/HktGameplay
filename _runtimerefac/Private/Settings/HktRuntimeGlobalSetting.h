// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/World.h"
#include "HktRuntimeGlobalSetting.generated.h"

/**
 * HktRuntime 전역 설정
 * Project Settings -> Game -> Hkt Runtime Settings 에서 편집 가능
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hkt Runtime Settings"))
class HKTRUNTIME_API UHktRuntimeGlobalSetting : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktRuntimeGlobalSetting();

	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Game"); }
	virtual FName GetSectionName() const override { return FName("HktRuntime"); }

	// === 로그인 / 레벨 전환 ===

	/** 로그인 맵 (Soft Path, 예: /Game/Login/Maps/LoginMap.LoginMap) */
	UPROPERTY(Config, EditAnywhere, Category = "Login", meta = (DisplayName = "Login Map"))
	TSoftObjectPtr<UWorld> LoginMap;

	/** 인게임 맵 (로그인 성공 후 이동할 맵 Soft Path, 예: /Game/TopDown/Maps/TopDownMap.TopDownMap) */
	UPROPERTY(Config, EditAnywhere, Category = "Login", meta = (DisplayName = "In-Game Map"))
	TSoftObjectPtr<UWorld> InGameMap;
};
