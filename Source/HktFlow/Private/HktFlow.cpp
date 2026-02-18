// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFlow.h"
#include "HktFlowDefinitions.h"

IMPLEMENT_MODULE(FHktFlowModule, HktFlow)

void FHktFlowModule::StartupModule()
{
	// 모듈 진입 시 모든 Flow 정의를 자동으로 등록
	FlowDefinitions::RegisterAllFlows();
	
	UE_LOG(LogTemp, Log, TEXT("[HktFlow] Module started - All flows registered"));
}

void FHktFlowModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("[HktFlow] Module shutdown"));
}
