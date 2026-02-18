// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * HktFlow Module
 * 
 * Flow 정의를 자동으로 등록하는 모듈입니다.
 * 모듈이 로드되면 모든 Flow 정의가 자동으로 등록됩니다.
 */
class FHktFlowModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
