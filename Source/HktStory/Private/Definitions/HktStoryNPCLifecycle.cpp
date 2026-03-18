// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryNPCLifecycle
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_NPC_Lifecycle, "Flow.NPC.Lifecycle", "NPC lifecycle management (death/despawn).");

	// State Tags — AnimInstance가 태그를 보고 애니메이션을 자동 재생
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Action_Death, "Anim.FullBody.Action.Death", "Death state tag.");

	// Loot
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_NPCLoot, "Entity.Item.NPCLoot", "Generic NPC loot drop.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_Material, "Tag.Item.Material", "Material item tag.");

	/**
	 * ================================================================
	 * NPC 생명주기 Flow
	 *
	 * 자연어로 읽으면:
	 * "1초마다 체력을 확인한다.
	 *  체력이 0 이하이면 죽음 애니메이션을 재생하고
	 *  NPC 위치에 전리품을 드랍한 뒤
	 *  3초 후 엔티티를 제거한다."
	 *
	 * NPC 스폰 시 함께 fire되어야 함.
	 * Self = NPC 엔티티
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_NPC_Lifecycle)
			.Label(TEXT("check"))
				.LoadEntityProperty(R0, Self, PropertyId::Health)
				.LoadConst(R1, 0)
				.CmpLe(Flag, R0, R1)                    // Health <= 0?
				.JumpIf(Flag, TEXT("die"))
				.WaitSeconds(1.0f)
				.Jump(TEXT("check"))

			.Label(TEXT("die"))
				.Log(TEXT("NPC died"))

				// 전리품 드랍
				.SpawnEntity(Entity_Item_NPCLoot)
				.SaveConst(Spawned, PropertyId::ItemState, 0)                // Ground
				.SaveConst(Spawned, PropertyId::ItemId, 201)                 // Loot ID
				.SaveConst(Spawned, PropertyId::ActionSlot, -1)              // 미등록
				.GetPosition(R3, Self)
				.SetPosition(Spawned, R3)                                    // NPC 위치에 드랍
				.AddTag(Spawned, Tag_Item_Material)

				// 죽음 상태 태그 추가 → AnimInstance가 태그를 감지하여 죽음 애니메이션 자동 재생
				.AddTag(Self, Tag_Anim_FullBody_Action_Death)
				.WaitSeconds(3.0f)
				.DestroyEntity(Self)
				.Halt()
			.BuildAndRegister();
	}
}
