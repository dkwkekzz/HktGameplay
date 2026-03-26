// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryHit
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Hit, "Story.Event.Hit.Basic", "Basic hit story — damage + VFX + reaction.");

	// 피격 애니메이션 (트리거 — fire-and-forget)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_HitReaction, "Anim.Montage.HitReaction", "Hit reaction montage trigger tag.");

	// 히트 이펙트 엔티티
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_HitEffect, "Entity.Effect.Hit", "Hit effect entity spawned at impact point.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.Niagara.HitSpark", "Melee hit spark VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/**
	 * ================================================================
	 * Hit Story
	 *
	 * BasicAttack에서 DispatchEventTo(Hit)로 발행됨.
	 * - Source = 공격자, Target = 피격 대상
	 *
	 * 자연어로 읽으면:
	 * "공격자의 공격력으로 피격 대상에게 데미지를 적용한다.
	 *  피격 위치에 히트 이펙트를 생성하고, 대상에 피격 모션을 재생한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_Hit)
			.Log(TEXT("Hit: 피격 처리 시작"))

			// === 1. 데미지 적용 ===
			// Self = 공격자 (SourceEntity), Target = 피격 대상 (TargetEntity)
			.LoadStore(R0, PropertyId::AttackPower)      // R0 = 공격자의 AttackPower
			.ApplyDamage(Target, R0)
			.PlaySound(Sound_Hit)

			// === 2. 히트 이펙트 엔티티 스폰 (피격 위치에 VFX) ===
			.SpawnEntity(Entity_HitEffect)
			.GetPosition(R2, Target)                     // R2 = 피격자 위치
			.SetPosition(Spawned, R2)                    // 이펙트를 피격 위치에 배치
			.PlayVFXAttached(Spawned, VFX_HitSpark)

			// === 3. 피격 대상에 히트리액션 (fire-and-forget) ===
			.AddTag(Target, Tag_Anim_Montage_HitReaction)

			// === 4. 히트 이펙트 엔티티 제거 ===
			.DestroyEntity(Spawned)

			.Log(TEXT("Hit: 피격 처리 완료"))
			.Halt()
		.BuildAndRegister();
	}
}
