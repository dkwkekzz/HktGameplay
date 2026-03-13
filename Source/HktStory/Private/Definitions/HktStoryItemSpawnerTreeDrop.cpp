// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryItemSpawnerTreeDrop
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Spawner_Item_TreeDrop, "Flow.Spawner.Item.TreeDrop", "Natural item spawner - tree drops.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_Wood, "Entity.Item.Wood", "Wood material item.");

	// Tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_Material, "Item.Material", "Material item category.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_Wood, "Item.Wood", "Wood item tag.");

	/**
	 * ================================================================
	 * 자연 아이템 스폰 (EntitySpawner 패턴)
	 *
	 * 자연어로 읽으면:
	 * "플레이어가 있을 때 30초마다 나무 아이템을 월드에 드랍한다.
	 *  아이템 수가 상한에 도달하면 대기한다."
	 *
	 * 서버가 EventTag "Flow.Spawner.Item.TreeDrop" fire.
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

				// 아이템 생성 (Equipment 엔티티)
				.SpawnEntity(HktType::Equipment, Entity_Item_Wood)
				.LoadConst(R2, 0)
				.SaveEntityProperty(Spawned, PropertyId::ItemState, R2)     // Ground
				.LoadConst(R2, 101)
				.SaveEntityProperty(Spawned, PropertyId::ItemId, R2)        // Wood = 101
				.LoadConst(R2, -1)
				.SaveEntityProperty(Spawned, PropertyId::ActionSlot, R2)    // 미등록
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
