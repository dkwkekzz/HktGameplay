// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "IHktUIModule.h"
#include "Modules/ModuleManager.h"

class FHktUIModule : public IHktUIModule
{
public:
	virtual void StartupModule() override { UE_LOG(LogTemp, Log, TEXT("HktUI Module Started")); }
	virtual void ShutdownModule() override { UE_LOG(LogTemp, Log, TEXT("HktUI Module Shutdown")); }
};

IMPLEMENT_MODULE(FHktUIModule, HktUI)
