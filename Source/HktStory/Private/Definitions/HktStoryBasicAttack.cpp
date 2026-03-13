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

	// State Tags — AnimInstance가 태그를 보고 애니메이션을 자동 재생
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_UpperBody_Combat_Attack, "Anim.UpperBody.Combat.Attack", "Basic attack upper body state tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage state tag.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.HitSpark", "Melee hit spark VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/**
	 * ================================================================
	 * 기본 공격 Flow
	 *
	 * 자연어로 읽으면:
	 * "공격 상태 태그를 추가하면 AnimInstance가 자동으로 애니메이션을 재생한다.
	 *  대상에게 공격력만큼 피해를 준다.
	 *  완료 후 공격 상태 태그를 제거한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_BasicAttack)
			.Log(TEXT("BasicAttack: 공격 시작"))

			// 타겟 로드 (IntentEvent에서)
			.LoadStore(Target, PropertyId::Param0)      // Param0 = 타겟 EntityId

			// 공격 상태 태그 추가 → AnimInstance가 태그를 감지하여 애니메이션 자동 재생
			.AddTag(Self, Tag_Anim_UpperBody_Combat_Attack)
			.AddTag(Self, Tag_Anim_Montage_Attack)
			.WaitAnimEnd(Self)

			// 공격력 로드
			.LoadStore(R0, PropertyId::AttackPower)

			// 피해 적용
			.ApplyDamage(Target, R0)
			.PlayVFXAttached(Target, VFX_HitSpark)
			.PlaySound(Sound_Hit)

			// 공격 상태 태그 제거
			.RemoveTag(Self, Tag_Anim_UpperBody_Combat_Attack)
			.RemoveTag(Self, Tag_Anim_Montage_Attack)

			.Log(TEXT("BasicAttack: 완료"))
			.Halt()
			.BuildAndRegister();
	}
}
