// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryBasicAttack
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_BasicAttack, "Ability.Attack.Basic", "Basic attack ability flow.");

	// Anim — 태그 계층에 레이어 포함
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_UpperBody_Combat_Attack, "Anim.UpperBody.Combat.Attack", "Basic attack upper body animation.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.HitSpark", "Melee hit spark VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/**
	 * ================================================================
	 * 기본 공격 Flow
	 *
	 * 자연어로 읽으면:
	 * "상체에 공격 애니메이션을 재생하고 (하체는 이동 유지),
	 *  애니메이션이 끝나면 대상에게 공격력만큼 피해를 준다.
	 *  완료 후 상체 애니메이션을 초기화한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_BasicAttack)
			.Log(TEXT("BasicAttack: 공격 시작"))

			// 타겟 로드 (IntentEvent에서)
			.LoadStore(Target, PropertyId::Param0)      // Param0 = 타겟 EntityId

			// 상체에 공격 애니메이션 (하체 이동 유지) — 태그 계층에서 레이어 자동 감지
			.PlayAnim(Self, Anim_UpperBody_Combat_Attack)
			.PlayAnimMontage(Self, Anim_Montage_Attack)
			.WaitAnimEnd(Self)

			// 공격력 로드
			.LoadStore(R0, PropertyId::AttackPower)

			// 피해 적용
			.ApplyDamage(Target, R0)
			.PlayVFXAttached(Target, VFX_HitSpark)
			.PlaySound(Sound_Hit)

			// 상체 애니메이션 초기화
			.StopAnim(Self, Anim_UpperBody_Combat_Attack)

			.Log(TEXT("BasicAttack: 완료"))
			.Halt()
			.BuildAndRegister();
	}
}
