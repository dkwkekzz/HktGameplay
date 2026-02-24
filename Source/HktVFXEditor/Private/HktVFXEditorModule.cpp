// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktVFXEditorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktVFXEditorModule"

class FHktVFXEditorModule : public IHktVFXEditorModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogTemp, Log, TEXT("HktVFXEditor Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogTemp, Log, TEXT("HktVFXEditor Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktVFXEditorModule, HktVFXEditor)

#undef LOCTEXT_NAMESPACE
