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

	/**
	 * ================================================================
	 * 아이템 스킬 사용 Flow (Story.Event.Combat.UseItemSkill)
	 *
	 * 자연어로 읽으면:
	 * "우클릭으로 아이템 스킬을 사용한다.
	 *  활성 슬롯(ActionSlot 0)의 아이템을 찾아
	 *  공속 기반 쿨타임(NextActionFrame)과 CP를 검증한 뒤,
	 *  CP를 차감하고 스킬 로직(대상에 공격력*2 피해)을 실행한다.
	 *  스킬의 후딜레이를 AttackSpeed로 나눠 NextActionFrame을 갱신한다."
	 *
	 * Self = 캐릭터, Target = 공격 대상 (Param0에서 로드)
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Event_Combat_UseItemSkill)
			.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity))
					return false;

				// 공속 기반 쿨타임 검증
				int32 NextFrame = WS.GetProperty(E.SourceEntity, PropertyId::NextActionFrame);
				if (WS.FrameNumber < NextFrame)
					return false;

				// ActionSlot 0에 활성 아이템이 있는지 찾기
				bool bFoundItem = false;
				int32 CpCost = 0;
				WS.ForEachEntityByOwner(WS.GetOwnerUid(E.SourceEntity), [&](FHktEntityId ItemId, int32 /*Slot*/)
				{
					if (bFoundItem) return;
					if (WS.GetProperty(ItemId, PropertyId::OwnerEntity) != E.SourceEntity) return;
					if (WS.GetProperty(ItemId, PropertyId::ItemState) != 2) return;     // Active만
					if (WS.GetProperty(ItemId, PropertyId::ActionSlot) != 0) return;     // 주무기 슬롯만
					CpCost = WS.GetProperty(ItemId, PropertyId::SkillCPCost);
					bFoundItem = true;
				});

				if (!bFoundItem)
					return false;

				// CP 검증
				int32 CurrentCP = WS.GetProperty(E.SourceEntity, PropertyId::CP);
				return CurrentCP >= CpCost;
			})

			.Log(TEXT("UseItemSkill: 스킬 시작"))

			// === 공속 기반 쿨타임 검증 (서버 이중 검증) ===
			.GetWorldTime(R0)                                           // R0 = 현재 프레임
			.LoadStore(R1, PropertyId::NextActionFrame)                 // R1 = NextActionFrame
			.CmpLt(Flag, R0, R1)
			.JumpIf(Flag, TEXT("fail"))

			// === ActionSlot 0의 활성 아이템 검색 ===
			.FindByOwner(Self, Entity_Item)
			.LoadConst(R6, 0)                                           // R6 = 아이템 발견 플래그

		.Label(TEXT("find_loop"))
			.NextFound()
			.JumpIfNot(Flag, TEXT("find_done"))

			// Active(State==2) 확인
			.LoadEntityProperty(R3, Iter, PropertyId::ItemState)
			.LoadConst(R4, 2)
			.CmpNe(R5, R3, R4)
			.JumpIf(R5, TEXT("find_loop"))

			// ActionSlot == 0 확인 (주무기)
			.LoadEntityProperty(R3, Iter, PropertyId::ActionSlot)
			.LoadConst(R4, 0)
			.CmpNe(R5, R3, R4)
			.JumpIf(R5, TEXT("find_loop"))

			// 아이템 발견! R6 = 1, Target 레지스터에 Iter 복사 (아이템 엔티티)
			.LoadConst(R6, 1)
			.Move(R2, Iter)                                             // R2 = 아이템 엔티티 ID (나중에 사용)

		.Label(TEXT("find_done"))
			// 아이템 미발견 시 실패
			.LoadConst(R4, 0)
			.CmpEq(Flag, R6, R4)
			.JumpIf(Flag, TEXT("fail"))

			// === CP 검증 및 차감 ===
			// R2 = 아이템 엔티티
			.LoadEntityProperty(R3, R2, PropertyId::SkillCPCost)        // R3 = CP 소모량
			.LoadStore(R4, PropertyId::CP)                              // R4 = 현재 CP
			.CmpLt(Flag, R4, R3)                                        // CP < Cost?
			.JumpIf(Flag, TEXT("fail_cp"))

			// CP 차감
			.Sub(R4, R4, R3)                                            // R4 = CP - Cost
			.SaveStore(PropertyId::CP, R4)                              // CP 저장

			// === 타겟 로드 ===
			.LoadStore(Target, PropertyId::Param0)                      // Param0 = 공격 대상 EntityId

			// === 스킬 애니메이션 ===
			.AddTag(Self, Tag_Anim_UpperBody_Combat_Skill)
			.AddTag(Self, Tag_Anim_Montage_Skill)
			.WaitAnimEnd(Self)

			// === 스킬 데미지 (공격력 * 2) ===
			.LoadStore(R0, PropertyId::AttackPower)                     // R0 = 공격력
			.LoadConst(R1, 2)
			.Mul(R0, R0, R1)                                            // R0 = 공격력 * 2
			.ApplyDamage(Target, R0)
			.PlayVFXAttached(Target, VFX_SkillHit)
			.PlaySound(Sound_SkillHit)

			// === NextActionFrame 갱신 (공속 기반) ===
			// NextActionFrame = 현재프레임 + (RecoveryFrame * 100 / AttackSpeed)
			.GetWorldTime(R0)                                           // R0 = 현재 프레임
			.LoadEntityProperty(R1, R2, PropertyId::RecoveryFrame)      // R1 = 아이템의 기본 후딜레이
			.LoadConst(R3, 100)
			.Mul(R1, R1, R3)                                            // R1 = RecoveryFrame * 100
			.LoadStore(R3, PropertyId::AttackSpeed)                     // R3 = AttackSpeed
			.Div(R1, R1, R3)                                            // R1 = delay (프레임 수)
			.Add(R0, R0, R1)                                            // R0 = 현재프레임 + delay
			.SaveStore(PropertyId::NextActionFrame, R0)

			// 스킬 태그 제거
			.RemoveTag(Self, Tag_Anim_UpperBody_Combat_Skill)
			.RemoveTag(Self, Tag_Anim_Montage_Skill)

			.Log(TEXT("UseItemSkill: 완료"))
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
