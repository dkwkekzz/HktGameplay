// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "HktStoryTags.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetNPC.h"
#include "Snippets/HktSnippetItem.h"

namespace HktStoryItemSpawnerBandage
{
	using namespace HktStoryTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Spawner_Item_Bandage, "Story.Flow.Spawner.Item.Bandage", "Bandage item spawner.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_Bandage, "Entity.Item.Bandage", "Bandage item — Heal skill.");

	// Tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_Bandage, "Entity.Attr.Item.Bandage", "Bandage item tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Heal, "Entity.Attr.Skill.Heal", "Item skill identifier: Heal.");

	// Skill Story reference
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Heal_Ref, "Story.Event.Combat.UseItemSkill", "Generic item skill story.");

	/**
	 * ================================================================
	 * 붕대 아이템 스폰 (EntitySpawner 패턴)
	 *
	 * 자연어로 읽으면:
	 * "플레이어가 있을 때 60초마다 붕대를 월드에 드랍한다.
	 *  아이템 수가 상한(1개)에 도달하면 대기한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_Spawner_Item_Bandage);
		B.Log(TEXT("Bandage spawner: activated"));

		// 주기적 스포너 루프 시작 (플레이어 체크 + 아이템 1개 상한)
		HktSnippetNPC::SpawnerLoopBegin(B, TEXT("loop"), TEXT("wait"), Tag_Item_Bandage, 1);

		// 아이템 엔티티 생성
		HktSnippetItem::SpawnGroundItem(B, Entity_Item_Bandage, { 202 }, Self);

		B	// 아이템 속성 설정 — 붕대는 공격력 없음, 자가 회복용
			.SaveConstEntity(Spawned, PropertyId::AttackPower, 0)
			.SetItemSkillTag(Spawned, Skill_Heal_Ref)
			.SaveConstEntity(Spawned, PropertyId::SkillCPCost, 40)
			.SaveConstEntity(Spawned, PropertyId::RecoveryFrame, 120)
			.SaveConstEntity(Spawned, PropertyId::SkillTargetRequired, 0)       // 셀프 힐
			.AddTag(Spawned, Tag_Item_Bandage)
			.AddTag(Spawned, Tag_Skill_Heal)

			.Log(TEXT("Bandage: item spawned"));

		// 주기적 스포너 루프 종결 (60초 대기)
		HktSnippetNPC::SpawnerLoopEnd(B, TEXT("loop"), TEXT("wait"), 60.0f);

		B.BuildAndRegister();
	}
}
