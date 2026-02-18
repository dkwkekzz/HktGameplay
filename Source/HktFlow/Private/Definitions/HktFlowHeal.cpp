// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktPropertyIds.h"
#include "HktFlowRegistry.h"
#include "NativeGameplayTags.h"

namespace HktFlowHeal
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_Heal, "Ability.Skill.Heal", "Heal skill ability flow.");

	// Anim
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_CastHeal, "Anim.CastHeal", "Heal cast animation.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HealCast, "VFX.HealCast", "Heal cast VFX.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HealBurst, "VFX.HealBurst", "Heal burst VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Heal, "Sound.Heal", "Heal sound.");

	/**
	 * ================================================================
	 * 추가 예제: 회복 스킬 Flow
	 *
	 * 자연어로 읽으면:
	 * "시전 애니메이션을 재생하고, 자신의 체력을 회복량만큼 회복한다.
	 *  체력이 최대치를 넘지 않도록 한다."
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;

		Flow(Flow_Heal)
			.Log(TEXT("Heal: 시전 시작"))

			// 시전 애니메이션
			.PlayAnim(Self, Anim_CastHeal)
			.PlayVFXAttached(Self, VFX_HealCast)
			.WaitSeconds(0.8f)

			// 현재 체력과 최대 체력 로드
			.LoadStore(R0, PropertyId::Health)
			.LoadStore(R1, PropertyId::MaxHealth)

			// 회복량 (Param0에서, 기본 50)
			.LoadStore(R2, PropertyId::Param0)
			.CmpEq(R3, R2, R3)                          // R2 == 0?
			.JumpIfNot(R3, TEXT("HasHealAmount"))
			.LoadConst(R2, 50)                          // 기본값 50
			.Label(TEXT("HasHealAmount"))

			// 새 체력 = 현재 + 회복량
			.Add(R0, R0, R2)

			// 최대 체력 제한
			.CmpGt(R3, R0, R1)                          // 새 체력 > 최대?
			.JumpIfNot(R3, TEXT("NoClamp"))
			.Move(R0, R1)                               // 최대로 제한
			.Label(TEXT("NoClamp"))

			// 체력 저장
			.SaveStore(PropertyId::Health, R0)

			// 회복 이펙트
			.PlayVFXAttached(Self, VFX_HealBurst)
			.PlaySound(Sound_Heal)

			.Log(TEXT("Heal: 완료"))
			.Halt()
			.BuildAndRegister();
	}
}
