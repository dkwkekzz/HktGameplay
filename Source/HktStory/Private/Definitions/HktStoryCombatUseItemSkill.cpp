// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "HktStoryTags.h"
#include "HktRuntimeTags.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetCombat.h"
#include "Snippets/HktSnippetItem.h"

namespace HktStoryCombatUseItemSkill
{
	using namespace HktStoryTags;
	using namespace HktGameplayTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_UseItemSkill, "Story.Event.Combat.UseItemSkill",
		"Use active item skill — CP cost, attack speed cooldown, then delegate to item's skill tag.");

	// Anim
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_UpperBody_Combat_Skill, "Anim.UpperBody.Combat.Skill",
		"Item skill upper body state tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Skill, "Anim.Montage.Skill",
		"Item skill montage state tag.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_SkillHit, "VFX.Niagara.SkillHit", "Item skill hit VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_SkillHit, "Sound.SkillHit", "Item skill hit sound.");

	// === 아이템 스킬 식별 태그 (dispatch 분기용) ===
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Fireball,  "Entity.Attr.Skill.Fireball",  "Item skill identifier: Fireball.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Heal,      "Entity.Attr.Skill.Heal",      "Item skill identifier: Heal.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Lightning,  "Entity.Attr.Skill.Lightning",  "Item skill identifier: Lightning.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Buff,      "Entity.Attr.Skill.Buff",      "Item skill identifier: Buff.");

	// === Dispatch 대상 Story 태그 ===
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Fireball,  "Story.Event.Skill.Fireball",  "Fireball skill story.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Heal,      "Story.Event.Skill.Heal",      "Heal skill story.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Lightning,  "Story.Event.Skill.Lightning",  "Lightning skill story.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Buff,      "Story.Event.Skill.Buff",      "Buff skill story.");

	/** EquipSlot[N] PropertyId 테이블 — Param1(슬롯 인덱스)에서 프로퍼티 ID를 결정 */
	static constexpr uint16 EquipSlotProperties[] =
	{
		PropertyId::EquipSlot0, PropertyId::EquipSlot1, PropertyId::EquipSlot2,
		PropertyId::EquipSlot3, PropertyId::EquipSlot4, PropertyId::EquipSlot5,
		PropertyId::EquipSlot6, PropertyId::EquipSlot7, PropertyId::EquipSlot8,
	};

	/**
	 * ================================================================
	 * 아이템 스킬 사용 Flow (Story.Event.Combat.UseItemSkill)
	 *
	 * 자연어로 읽으면:
	 * "슬롯 키를 눌러 아이템 스킬을 사용한다.
	 *  Param1(슬롯 인덱스)의 EquipSlot에서 아이템을 찾아
	 *  공속 기반 쿨타임(NextActionFrame)과 CP를 검증한 뒤,
	 *  CP를 차감하고 NextActionFrame을 갱신한다.
	 *  아이템에 고유 스킬 태그가 있으면 해당 스킬 Story로 디스패치하고,
	 *  없으면 기본 일괄 데미지(공격력*2)를 실행한다."
	 *
	 * Self = 캐릭터, Param0 = 공격 대상 EntityId, Param1 = 슬롯 인덱스
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Event_Combat_UseItemSkill);
		B.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity))
					return false;

				// 공속 기반 쿨타임 검증
				int32 NextFrame = WS.GetProperty(E.SourceEntity, PropertyId::NextActionFrame);
				if (WS.FrameNumber < NextFrame)
					return false;

				// Param1 = 슬롯 인덱스 → EquipSlot[N]에서 아이템 EntityId 조회
				int32 SlotIndex = E.Param1;
				if (SlotIndex < 0 || SlotIndex >= UE_ARRAY_COUNT(EquipSlotProperties))
					return false;

				FHktEntityId ItemId = WS.GetProperty(E.SourceEntity, EquipSlotProperties[SlotIndex]);
				if (!WS.IsValidEntity(ItemId))
					return false;

				// CP 검증
				int32 CpCost = WS.GetProperty(ItemId, PropertyId::SkillCPCost);
				int32 CurrentCP = WS.GetProperty(E.SourceEntity, PropertyId::CP);
				return CurrentCP >= CpCost;
			})
			.Log(TEXT("UseItemSkill: 스킬 시작"));

		// === 공속 기반 쿨타임 검증 (서버 이중 검증) ===
		HktSnippetCombat::CooldownCheck(B, TEXT("fail"));

		// === Param1(슬롯 인덱스)로 EquipSlot[N]에서 아이템 엔티티 조회 ===
		HktSnippetItem::LoadItemFromSlot(B, R2, TEXT("fail"));

		B	// === CP 검증 및 차감 ===
			.LoadEntityProperty(R3, R2, PropertyId::SkillCPCost)        // R3 = CP 소모량
			.LoadStore(R4, PropertyId::CP)                              // R4 = 현재 CP
			.CmpLt(Flag, R4, R3)                                        // CP < Cost?
			.JumpIf(Flag, TEXT("fail_cp"))

			// CP 차감
			.Sub(R4, R4, R3)                                            // R4 = CP - Cost
			.SaveStore(PropertyId::CP, R4)                              // CP 저장

			// === 타겟 로드 ===
			.LoadStore(Target, PropertyId::Param0);                     // Param0 = 공격 대상 EntityId

		// === NextActionFrame 갱신 (공속 기반) — dispatch 전에 수행 ===
		HktSnippetCombat::CooldownUpdateFromEntity(B, R2);

		// === 아이템 스킬 태그 확인 → 고유 스킬 dispatch ===
		B	.HasTag(R5, R2, Tag_Skill_Fireball)
			.JumpIf(R5, TEXT("dispatch_fireball"))

			.HasTag(R5, R2, Tag_Skill_Heal)
			.JumpIf(R5, TEXT("dispatch_heal"))

			.HasTag(R5, R2, Tag_Skill_Lightning)
			.JumpIf(R5, TEXT("dispatch_lightning"))

			.HasTag(R5, R2, Tag_Skill_Buff)
			.JumpIf(R5, TEXT("dispatch_buff"))

			// === 기본 일괄 데미지 (고유 스킬 태그 없는 아이템 — 목검 등) ===
			.AddTag(Self, Tag_Anim_UpperBody_Combat_Skill)
			.AddTag(Self, Tag_Anim_Montage_Skill)
			.WaitAnimEnd(Self)

			.LoadStore(R0, PropertyId::AttackPower)                     // R0 = 공격력
			.LoadConst(R1, 2)
			.Mul(R0, R0, R1)                                            // R0 = 공격력 * 2
			.ApplyDamage(Target, R0)
			.PlayVFXAttached(Target, VFX_SkillHit)
			.PlaySound(Sound_SkillHit)

			.RemoveTag(Self, Tag_Anim_UpperBody_Combat_Skill)
			.RemoveTag(Self, Tag_Anim_Montage_Skill)

			.Log(TEXT("UseItemSkill: 기본 스킬 완료"))
			.Halt()

		// === Dispatch 분기 ===
		.Label(TEXT("dispatch_fireball"))
			.Log(TEXT("UseItemSkill: → Fireball"))
			.DispatchEvent(Story_Fireball)
			.Halt()

		.Label(TEXT("dispatch_heal"))
			.Log(TEXT("UseItemSkill: → Heal"))
			.DispatchEvent(Story_Heal)
			.Halt()

		.Label(TEXT("dispatch_lightning"))
			.Log(TEXT("UseItemSkill: → Lightning"))
			.DispatchEvent(Story_Lightning)
			.Halt()

		.Label(TEXT("dispatch_buff"))
			.Log(TEXT("UseItemSkill: → Buff"))
			.DispatchEvent(Story_Buff)
			.Halt()

		.Label(TEXT("fail_cp"))
			.Log(TEXT("UseItemSkill: CP 부족 — 실패"))
			.Fail()

		.Label(TEXT("fail"))
			.Log(TEXT("UseItemSkill: 사전조건 위반 — 실패"))
			.Fail()
		.BuildAndRegister();
	}
}
