// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class HKTUI_API IHktUIModule : public IModuleInterface
{
public:
	static inline IHktUIModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktUIModule>("HktUI");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktUI");
	}
};
