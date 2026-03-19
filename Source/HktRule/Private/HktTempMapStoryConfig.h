// Copyright Hkt Studios, Inc. All Rights Reserved.
// 임시 맵 스토리 설정 — MapGenerator 연동 전까지 테스트용.
// TODO: MapGenerator의 FHktMapData 연동 후 이 파일 삭제.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

/**
 * 임시 스토리 엔트리 — FHktMapStoryRef + 위치 파라미터.
 * MapGenerator의 FHktMapStoryRef와 1:1 대응.
 */
struct FHktTempStoryEntry
{
	FGameplayTag StoryTag;
	int32 SpawnPosX = 0;
	int32 SpawnPosY = 0;
};

/**
 * 테스트 맵의 스토리 구성.
 * 향후 FHktMapData::GlobalStories + FHktMapRegion::Stories로 교체.
 */
namespace HktTempMapStoryConfig
{
	// Story 태그 — HktStory 모듈의 정의와 동일 문자열
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_Spawner_GoblinCamp,    "Story.Flow.Spawner.GoblinCamp",       "Periodic goblin camp spawner.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_Spawner_Item_TreeDrop, "Story.Flow.Spawner.Item.TreeDrop",    "Tree drop item spawner.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_Spawner_Wave_Arena,    "Story.Flow.Spawner.Wave.Arena",       "Wave arena spawner.");

	/**
	 * 그룹별 스포너 목록 반환.
	 * GroupIndex로 그룹별 차별화 가능 — Region 개념과 대응.
	 */
	inline TArray<FHktTempStoryEntry> GetSpawnersForGroup(int32 GroupIndex)
	{
		TArray<FHktTempStoryEntry> Out;

		// 전역 스토리 — 모든 그룹 공통 (FHktMapData::GlobalStories 대응)
		Out.Add({ Flow_Spawner_GoblinCamp,    1000 + GroupIndex * 500, 1000 });
		Out.Add({ Flow_Spawner_Item_TreeDrop,  1200 + GroupIndex * 500,  800 });

		// 그룹 0 전용 — Region별 스토리 (FHktMapRegion::Stories 대응)
		if (GroupIndex == 0)
		{
			Out.Add({ Flow_Spawner_Wave_Arena, 2000, 2000 });
		}

		return Out;
	}
}
