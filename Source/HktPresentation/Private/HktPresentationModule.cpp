// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktPresentationModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktPresentationModule"

class FHktPresentationModule : public IHktPresentationModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogTemp, Log, TEXT("HktPresentation Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogTemp, Log, TEXT("HktPresentation Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktPresentationModule, HktPresentation)

#undef LOCTEXT_NAMESPACE
