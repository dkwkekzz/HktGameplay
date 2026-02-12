// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktUIGlobalSetting.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hkt UI Settings"))
class HKTUI_API UHktUIGlobalSetting : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Game"); }
	virtual FName GetSectionName() const override { return FName("HktUI"); }
};
