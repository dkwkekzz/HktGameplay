// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryModule.h"
#include "HktStoryRegistry.h"
#include "HktCoreEventLog.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktStory, Log, All); // Story 모듈은 단일 파일이므로 static 유지

IMPLEMENT_MODULE(FHktStoryModule, HktStory)

void FHktStoryModule::StartupModule()
{
	// 모듈 진입 시 모든 Story 정의를 자동으로 등록
	// 각 Story의 .cpp 파일에서 정적 초기화를 통해 자동으로 레지스트리에 등록되었으므로,
	// 여기서는 등록된 모든 Story를 초기화만 하면 됩니다.
	FHktStoryRegistry::InitializeAllStories();
	HKT_EVENT_LOG(HktLogTags::Story, EHktLogLevel::Info, EHktLogSource::Server, TEXT("HktStory module started"));
}

void FHktStoryModule::ShutdownModule()
{
	UE_LOG(LogHktStory, Log, TEXT("HktStory Module Shutdown"));
}
