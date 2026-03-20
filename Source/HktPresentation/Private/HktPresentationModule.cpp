// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktPresentationModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHktPresentationModule"

DEFINE_LOG_CATEGORY_STATIC(LogHktPresentation, Log, All);

class FHktPresentationModule : public IHktPresentationModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogHktPresentation, Log, TEXT("HktPresentation Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogHktPresentation, Log, TEXT("HktPresentation Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FHktPresentationModule, HktPresentation)

#undef LOCTEXT_NAMESPACE
