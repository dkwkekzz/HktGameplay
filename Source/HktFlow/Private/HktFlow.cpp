// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktFlow.h"
#include "HktFlowRegistry.h"

IMPLEMENT_MODULE(FHktFlowModule, HktFlow)

void FHktFlowModule::StartupModule()
{
	// 모듈 진입 시 모든 Flow 정의를 자동으로 등록
	// 각 Flow의 .cpp 파일에서 정적 초기화를 통해 자동으로 레지스트리에 등록되었으므로,
	// 여기서는 등록된 모든 Flow를 초기화만 하면 됩니다.
	FHktFlowRegistry::InitializeAllFlows();
	
	UE_LOG(LogTemp, Log, TEXT("[HktFlow] Module started - All flows registered via self-registration"));
}

void FHktFlowModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("[HktFlow] Module shutdown"));
}
