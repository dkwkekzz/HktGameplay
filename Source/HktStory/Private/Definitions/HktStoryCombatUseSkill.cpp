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

namespace HktStoryCombatUseSkill
{
	using namespace HktStoryTags;
	using namespace HktGameplayTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_UseSkill, "Story.Event.Combat.UseSkill",
		"Unified skill pipeline — cooldown, CP (item), then dispatch to specific skill story or innate BasicAttack.");

	// Innate skill dispatch target
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_BasicAttack, "Story.Event.Attack.Basic", "Basic attack story (innate fallback).");

	// Anim (기본 일괄 데미지용)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_UpperBody_Combat_Skill, "Anim.UpperBody.Combat.Skill",
		"Item skill upper body state tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Skill, "Anim.Montage.Skill",
		"Item skill montage state tag.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_SkillHit, "VFX.Niagara.SkillHit", "Item skill hit VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_SkillHit, "Sound.SkillHit", "Item skill hit sound.");

	// === 아이템 스킬 식별 태그 (dispatch 분기용) ===
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Fireball,   "Entity.Attr.Skill.Fireball",   "Item skill identifier: Fireball.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Heal,       "Entity.Attr.Skill.Heal",       "Item skill identifier: Heal.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Lightning,   "Entity.Attr.Skill.Lightning",   "Item skill identifier: Lightning.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Skill_Buff,       "Entity.Attr.Skill.Buff",       "Item skill identifier: Buff.");

	// === Dispatch 대상 Story 태그 ===
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Fireball,   "Story.Event.Skill.Fireball",   "Fireball skill story.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Heal,       "Story.Event.Skill.Heal",       "Heal skill story.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Lightning,   "Story.Event.Skill.Lightning",   "Lightning skill story.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Buff,       "Story.Event.Skill.Buff",       "Buff skill story.");

	/** EquipSlot[N] PropertyId 테이블 */
	static constexpr uint16 EquipSlotProperties[] =
	{
		PropertyId::EquipSlot0, PropertyId::EquipSlot1, PropertyId::EquipSlot2,
		PropertyId::EquipSlot3, PropertyId::EquipSlot4, PropertyId::EquipSlot5,
		PropertyId::EquipSlot6, PropertyId::EquipSlot7, PropertyId::EquipSlot8,
	};

	/** 기본 공격 (innate) 후딜레이 — BasicAttack과 동일 */
	static constexpr int32 InnateRecoveryFrame = 30;

	/**
	 * ================================================================
	 * 통합 스킬 사용 Flow (Story.Event.Combat.UseSkill)
	 *
	 * 자연어로 읽으면:
	 * "쿨타임을 확인한다. Param1 슬롯에 아이템이 있으면 아이템 스킬을 사용한다.
	 *  CP를 검증하고 차감한 뒤, 아이템 태그에 따라 고유 스킬 Story로 디스패치한다.
	 *  태그가 없으면 기본 일괄 데미지(공격력*2)를 실행한다.
	 *  슬롯에 아이템이 없으면 캐릭터 본연의 기본 공격(BasicAttack)을 디스패치한다."
	 *
	 * 두 가지 진입 경로:
	 * 1. 슬롯 키: Param0 = 타겟 EntityId, Param1 = 슬롯 인덱스
	 * 2. AttackEngage: Target 레지스터에 타겟, Param1 = 0 (기본 슬롯)
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Event_Combat_UseSkill);
		B.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity))
					return false;

				// 공속 기반 쿨타임 검증
				int32 NextFrame = WS.GetProperty(E.SourceEntity, PropertyId::NextActionFrame);
				if (WS.FrameNumber < NextFrame)
					return false;

				// 아이템 슬롯 확인 — 아이템이 있으면 CP 검증, 없으면 본연 스킬(무조건 통과)
				int32 SlotIndex = E.Param1;
				if (SlotIndex >= 0 && SlotIndex < static_cast<int32>(UE_ARRAY_COUNT(EquipSlotProperties)))
				{
					FHktEntityId ItemId = WS.GetProperty(E.SourceEntity, EquipSlotProperties[SlotIndex]);
					if (WS.IsValidEntity(ItemId))
					{
						// 아이템 스킬 → CP 검증
						int32 CpCost = WS.GetProperty(ItemId, PropertyId::SkillCPCost);
						int32 CurrentCP = WS.GetProperty(E.SourceEntity, PropertyId::CP);
						return CurrentCP >= CpCost;
					}
				}

				// 아이템 없음 → 본연 스킬 (CP 불요)
				return true;
			})
			.Log(TEXT("UseSkill: 스킬 시작"));

		// === 공속 기반 쿨타임 검증 (서버 이중 검증) ===
		HktSnippetCombat::CooldownCheck(B, TEXT("fail"));

		// === 타겟 해석: Param0이 유효하면 사용, 아니면 기존 Target 레지스터 유지 ===
		B	.LoadStore(R5, PropertyId::Param0)
			.LoadConst(R6, 0)
			.CmpNe(Flag, R5, R6)
			.JumpIfNot(Flag, TEXT("target_resolved"))
			.Move(Target, R5)
		.Label(TEXT("target_resolved"));

		// === Param1(슬롯)로 아이템 로드 시도 — 실패 시 innate 경로 ===
		HktSnippetItem::LoadItemFromSlot(B, R2, TEXT("innate"));

		B	// === 아이템 스킬 경로: CP 검증 및 차감 ===
			.LoadEntityProperty(R3, R2, PropertyId::SkillCPCost)        // R3 = CP 소모량
			.LoadStore(R4, PropertyId::CP)                              // R4 = 현재 CP
			.CmpLt(Flag, R4, R3)                                        // CP < Cost?
			.JumpIf(Flag, TEXT("fail_cp"))

			// CP 차감
			.Sub(R4, R4, R3)
			.SaveStore(PropertyId::CP, R4);

		// === NextActionFrame 갱신 (아이템 RecoveryFrame 기반) ===
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

			.LoadStore(R0, PropertyId::AttackPower)
			.LoadConst(R1, 2)
			.Mul(R0, R0, R1)
			.ApplyDamage(Target, R0)
			.PlayVFXAttached(Target, VFX_SkillHit)
			.PlaySound(Sound_SkillHit)

			.RemoveTag(Self, Tag_Anim_UpperBody_Combat_Skill)
			.RemoveTag(Self, Tag_Anim_Montage_Skill)

			.Log(TEXT("UseSkill: 아이템 기본 스킬 완료"))
			.Halt()

		// === 고유 스킬 Dispatch 분기 ===
		.Label(TEXT("dispatch_fireball"))
			.Log(TEXT("UseSkill: → Fireball"))
			.DispatchEvent(Story_Fireball)
			.Halt()

		.Label(TEXT("dispatch_heal"))
			.Log(TEXT("UseSkill: → Heal"))
			.DispatchEvent(Story_Heal)
			.Halt()

		.Label(TEXT("dispatch_lightning"))
			.Log(TEXT("UseSkill: → Lightning"))
			.DispatchEvent(Story_Lightning)
			.Halt()

		.Label(TEXT("dispatch_buff"))
			.Log(TEXT("UseSkill: → Buff"))
			.DispatchEvent(Story_Buff)
			.Halt()

		// === 본연 스킬 경로 (아이템 없음 → BasicAttack) ===
		.Label(TEXT("innate"))
			.Log(TEXT("UseSkill: → 본연 스킬 (BasicAttack)"));

		HktSnippetCombat::CooldownUpdateConst(B, InnateRecoveryFrame);

		B	.DispatchEvent(Story_BasicAttack)
			.Halt()

		.Label(TEXT("fail_cp"))
			.Log(TEXT("UseSkill: CP 부족 — 실패"))
			.Fail()

		.Label(TEXT("fail"))
			.Log(TEXT("UseSkill: 사전조건 위반 — 실패"))
			.Fail()
		.BuildAndRegister();
	}
}
