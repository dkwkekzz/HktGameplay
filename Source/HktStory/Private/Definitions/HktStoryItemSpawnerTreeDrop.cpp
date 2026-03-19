// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryItemSpawnerTreeDrop
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Spawner_Item_TreeDrop, "Story.Flow.Spawner.Item.TreeDrop", "Natural item spawner - tree drops.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_Wood, "Entity.Item.Wood", "Wood material item.");

	// Tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_Material, "Entity.Attr.Item.Material", "Material item category.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_Wood, "Entity.Attr.Item.Wood", "Wood item tag.");

	/**
	 * ================================================================
	 * 자연 아이템 스폰 (EntitySpawner 패턴)
	 *
	 * 자연어로 읽으면:
	 * "플레이어가 있을 때 30초마다 나무 아이템을 월드에 드랍한다.
	 *  아이템 수가 상한에 도달하면 대기한다."
	 *
	 * 서버가 EventTag "Story.Flow.Spawner.Item.TreeDrop" fire.
	 * Param0 = SpawnPosX, Param1 = SpawnPosY
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_Spawner_Item_TreeDrop)
			.Log(TEXT("TreeDrop spawner: activated"))

			.Label(TEXT("loop"))
				.HasPlayerInGroup(Flag)
				.JumpIfNot(Flag, TEXT("wait"))

				// 아이템 인구 체크
				.CountByTag(R0, Tag_Item_Wood)
				.LoadConst(R1, 10)
				.CmpGe(Flag, R0, R1)
				.JumpIf(Flag, TEXT("wait"))

				// 아이템 엔티티 생성
				.SpawnEntity(Entity_Item_Wood)
				.SaveConstEntity(Spawned, PropertyId::ItemState, 0)               // Ground
				.SaveConstEntity(Spawned, PropertyId::ItemId, 101)                // Wood = 101
				.SaveConstEntity(Spawned, PropertyId::ActionSlot, -1)             // 미등록
				.AddTag(Spawned, Tag_Item_Material)
				.AddTag(Spawned, Tag_Item_Wood)

				// 위치 설정
				.LoadStore(R3, PropertyId::Param0)
				.LoadStore(R4, PropertyId::Param1)
				.LoadConst(R5, 0)
				.SetPosition(Spawned, R3)

				.Log(TEXT("TreeDrop: wood spawned"))

			.Label(TEXT("wait"))
				.WaitSeconds(30.0f)
				.Jump(TEXT("loop"))
			.BuildAndRegister();
	}
}
