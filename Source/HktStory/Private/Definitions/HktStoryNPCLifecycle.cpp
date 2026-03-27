// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "HktStoryTags.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetItem.h"

namespace HktStoryNPCLifecycle
{
	using namespace HktStoryTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_NPC_Lifecycle, "Story.Flow.NPC.Lifecycle", "NPC lifecycle management (death/despawn).");

	// State Tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Action_Death, "Anim.FullBody.Action.Death", "Death state tag.");

	// === Loot Item Entities ===
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_AncientStaff,   "Entity.Item.AncientStaff",   "Ancient staff item — Fireball skill.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_Bandage,        "Entity.Item.Bandage",        "Bandage item — Heal skill.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_ThunderHammer,  "Entity.Item.ThunderHammer",  "Thunder hammer item — Lightning skill.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_WingsOfFreedom, "Entity.Item.WingsOfFreedom", "Wings of Freedom item — Buff skill.");

	// === 아이템 식별 태그 ===
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_AncientStaff,   "Entity.Attr.Item.AncientStaff",   "Ancient staff item tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_Bandage,        "Entity.Attr.Item.Bandage",        "Bandage item tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_ThunderHammer,  "Entity.Attr.Item.ThunderHammer",  "Thunder hammer item tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_WingsOfFreedom, "Entity.Attr.Item.WingsOfFreedom", "Wings of Freedom item tag.");

	// === 스킬 식별 태그 ===
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Fireball,  "Entity.Attr.Skill.Fireball",  "Item skill identifier: Fireball.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Heal,      "Entity.Attr.Skill.Heal",      "Item skill identifier: Heal.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Lightning,  "Entity.Attr.Skill.Lightning",  "Item skill identifier: Lightning.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Buff,      "Entity.Attr.Skill.Buff",      "Item skill identifier: Buff.");

	// === Skill Story reference (모든 아이템이 UseSkill을 통해 라우팅) ===
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_UseSkill_Ref, "Story.Event.Combat.UseSkill", "Unified skill pipeline story.");

	/**
	 * ================================================================
	 * NPC 생명주기 Flow
	 *
	 * 자연어로 읽으면:
	 * "사망 태그(Death)가 부여될 때까지 1초마다 확인한다.
	 *  사망이 감지되면 NPC 위치에 4종 아이템 중 랜덤 1개를
	 *  스킬 속성과 함께 드랍한다.
	 *  사망 애니메이션과 엔티티 제거는 Death 스토리에서 처리된다."
	 *
	 * NPC 스폰 시 함께 fire되어야 함.
	 * Self = NPC 엔티티
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_NPC_Lifecycle);
		B.Label(TEXT("check"))
				.HasTag(R0, Self, Tag_Anim_FullBody_Action_Death)
				.JumpIf(R0, TEXT("die"))
				.WaitSeconds(1.0f)
				.Jump(TEXT("check"))

			.Label(TEXT("die"))
				.Log(TEXT("NPC died — random loot drop"))

				// === 랜덤 아이템 선택 (0~3) ===
				.LoadConst(R0, 4)
				.RandomInt(R1, R0)                       // R1 = 0~3

				.LoadConst(R0, 0)
				.CmpEq(Flag, R1, R0)
				.JumpIf(Flag, TEXT("drop_staff"))

				.LoadConst(R0, 1)
				.CmpEq(Flag, R1, R0)
				.JumpIf(Flag, TEXT("drop_bandage"))

				.LoadConst(R0, 2)
				.CmpEq(Flag, R1, R0)
				.JumpIf(Flag, TEXT("drop_hammer"))

				// 기본(3) → 자유의 날개
				.Jump(TEXT("drop_wings"))

			// === 고대 지팡이 (ItemId=200, Fireball) ===
			.Label(TEXT("drop_staff"));

		HktSnippetItem::SpawnGroundItem(B, Entity_Item_AncientStaff, { 200 }, Self);
		B	.SaveConstEntity(Spawned, PropertyId::AttackPower, 8)
			.SetItemSkillTag(Spawned, Skill_UseSkill_Ref)
			.SaveConstEntity(Spawned, PropertyId::SkillCPCost, 50)
			.SaveConstEntity(Spawned, PropertyId::RecoveryFrame, 90)
			.SaveConstEntity(Spawned, PropertyId::SkillTargetRequired, 1)
			.SetStance(Spawned, HktStance::Spear)
			.AddTag(Spawned, Tag_Item_AncientStaff)
			.AddTag(Spawned, Tag_Skill_Fireball)
			.Jump(TEXT("after_drop"))

			// === 붕대 (ItemId=202, Heal) ===
			.Label(TEXT("drop_bandage"));

		HktSnippetItem::SpawnGroundItem(B, Entity_Item_Bandage, { 202 }, Self);
		B	.SaveConstEntity(Spawned, PropertyId::AttackPower, 0)
			.SetItemSkillTag(Spawned, Skill_UseSkill_Ref)
			.SaveConstEntity(Spawned, PropertyId::SkillCPCost, 40)
			.SaveConstEntity(Spawned, PropertyId::RecoveryFrame, 120)
			.SaveConstEntity(Spawned, PropertyId::SkillTargetRequired, 0)
			.AddTag(Spawned, Tag_Item_Bandage)
			.AddTag(Spawned, Tag_Skill_Heal)
			.Jump(TEXT("after_drop"))

			// === 천둥 망치 (ItemId=203, Lightning) ===
			.Label(TEXT("drop_hammer"));

		HktSnippetItem::SpawnGroundItem(B, Entity_Item_ThunderHammer, { 203 }, Self);
		B	.SaveConstEntity(Spawned, PropertyId::AttackPower, 12)
			.SetItemSkillTag(Spawned, Skill_UseSkill_Ref)
			.SaveConstEntity(Spawned, PropertyId::SkillCPCost, 60)
			.SaveConstEntity(Spawned, PropertyId::RecoveryFrame, 100)
			.SaveConstEntity(Spawned, PropertyId::SkillTargetRequired, 1)
			.SetStance(Spawned, HktStance::Sword1H)
			.AddTag(Spawned, Tag_Item_ThunderHammer)
			.AddTag(Spawned, Tag_Skill_Lightning)
			.Jump(TEXT("after_drop"))

			// === 자유의 날개 (ItemId=204, Buff) ===
			.Label(TEXT("drop_wings"));

		HktSnippetItem::SpawnGroundItem(B, Entity_Item_WingsOfFreedom, { 204 }, Self);
		B	.SaveConstEntity(Spawned, PropertyId::AttackPower, 0)
			.SetItemSkillTag(Spawned, Skill_UseSkill_Ref)
			.SaveConstEntity(Spawned, PropertyId::SkillCPCost, 30)
			.SaveConstEntity(Spawned, PropertyId::RecoveryFrame, 150)
			.SaveConstEntity(Spawned, PropertyId::SkillTargetRequired, 0)
			.AddTag(Spawned, Tag_Item_WingsOfFreedom)
			.AddTag(Spawned, Tag_Skill_Buff)

			// === 루트 드랍 완료 — 사망 처리는 Death 스토리에서 수행 ===
			.Label(TEXT("after_drop"))
				.Log(TEXT("NPC Lifecycle: 루트 드랍 완료"))
				.Halt()
			.BuildAndRegister();
	}
}
