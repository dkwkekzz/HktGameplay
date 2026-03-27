// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetCombat.h"

namespace HktStoryHitBasic
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_HitBasic, "Story.Event.Combat.Hit.Basic", "Basic attack hit story — damage + reaction + death check.");

	// Death Story (dispatch target)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Death, "Story.Event.Death", "Death story — death animation + fade + destroy.");

	// 피격 애니메이션
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_HitReaction, "Anim.Montage.HitReaction", "Hit reaction montage trigger tag.");

	// VFX / Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.Niagara.HitSpark", "Melee hit spark VFX.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/**
	 * ================================================================
	 * 일반 공격 히트 스토리
	 *
	 * 자연어로 읽으면:
	 * "공격자의 공격력으로 피격 대상에게 데미지를 부여한다.
	 *  피격 위치에 이펙트를 생성하고 피격 모션을 재생한다.
	 *  대상의 체력이 0 이하이면 Death 스토리를 부여한다."
	 *
	 * Self = 공격자 (SourceEntity)
	 * Target = 피격 대상 (TargetEntity)
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_HitBasic);
		B.Log(TEXT("HitBasic: 피격 처리"))

			// === 1. 데미지 부여 ===
			.LoadStore(R0, PropertyId::AttackPower)       // R0 = 공격자(Self)의 AttackPower
			.ApplyDamage(Target, R0)

			// === 2. 피격 이펙트 ===
			.GetPosition(R2, Target)
			.PlayVFX(R2, VFX_HitSpark)
			.PlaySound(Sound_Hit);

		// === 3. 피격 모션 ===
		HktSnippetCombat::AnimTrigger(B, Target, Tag_Anim_Montage_HitReaction);

		B	// === 4. 체력 확인 → 사망 판정 ===
			.LoadEntityProperty(R0, Target, PropertyId::Health)
			.LoadConst(R1, 0)
			.CmpLe(Flag, R0, R1)
			.JumpIfNot(Flag, TEXT("alive"))

			// 체력 0 이하 → Death 스토리 디스패치
			.DispatchEventTo(Story_Death, Target)

		.Label(TEXT("alive"))
			.Log(TEXT("HitBasic: 완료"))
			.Halt()
		.BuildAndRegister();
	}
}
