// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStory.h"
#include "HktStoryRegistry.h"

IMPLEMENT_MODULE(FHktStoryModule, HktStory)

void FHktStoryModule::StartupModule()
{
	// 모듈 진입 시 모든 Story 정의를 자동으로 등록
	// 각 Story의 .cpp 파일에서 정적 초기화를 통해 자동으로 레지스트리에 등록되었으므로,
	// 여기서는 등록된 모든 Story를 초기화만 하면 됩니다.
	FHktStoryRegistry::InitializeAllStories();

	UE_LOG(LogTemp, Log, TEXT("[HktStory] Module started - All stories registered via self-registration"));
}

void FHktStoryModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("[HktStory] Module shutdown"));
}
