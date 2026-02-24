// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * HktVFXEditor Module Interface (Editor-only)
 *
 * VFX 에셋/리소스 생성·편집용 에디터 전용 모듈.
 * HktVFX 런타임 모듈과 분리되어 에디터에서만 로드됨.
 */
class HKTVFXEDITOR_API IHktVFXEditorModule : public IModuleInterface
{
public:
	static inline IHktVFXEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IHktVFXEditorModule>("HktVFXEditor");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("HktVFXEditor");
	}
};
