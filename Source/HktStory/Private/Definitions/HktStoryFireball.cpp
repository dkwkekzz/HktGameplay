// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryFireball
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Fireball, "Ability.Skill.Fireball", "Fireball skill flow.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Projectile_Fireball, "Entity.Projectile.Fireball", "Fireball projectile entity.");

	// Anim — 태그 계층에 레이어 포함
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_UpperBody_Cast_Fireball, "Anim.UpperBody.Cast.Fireball", "Fireball cast animation.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_FireballLaunch, "Sound.FireballLaunch", "Fireball launch sound.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Explosion, "Sound.Explosion", "Explosion sound.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_DirectHit, "VFX.DirectHit", "Direct hit impact VFX.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_FireballExplosion, "VFX.FireballExplosion", "Fireball explosion VFX.");

	// Effect
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Burn, "Effect.Burn", "Burn effect: fire damage over time.");

	/**
	 * ================================================================
	 * 파이어볼 스킬 Flow
	 *
	 * 자연어로 읽으면:
	 * "상체에 시전 애니메이션을 재생하고 1초 기다린다.
	 *  파이어볼을 생성하여 앞으로 날린다.
	 *  충돌하면 파이어볼을 제거하고 직격 대상에게 100 피해를 준다.
	 *  주변 300 범위 내 대상들에게 각각 50 피해와 화상을 입힌다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_Fireball)
			// === 시전 시작 ===
			.Log(TEXT("Fireball: 시전 시작"))
			.PlayAnim(Self, Anim_UpperBody_Cast_Fireball)
			.WaitSeconds(1.0f)                          // 1초 대기

			// === 파이어볼 생성 및 발사 ===
			.Log(TEXT("Fireball: 투사체 생성"))
			.SpawnEntity(HktType::Projectile, Entity_Projectile_Fireball)

			// 파이어볼 위치를 시전자 위치로 설정
			.GetPosition(R0, Self)                      // R0,R1,R2 = 시전자 위치
			.SetPosition(Spawned, R0)                   // 파이어볼 위치 = 시전자 위치

			// 파이어볼을 전방으로 이동 (속도 500 cm/s)
			.MoveForward(Spawned, 500)
			.PlaySound(Sound_FireballLaunch)

			// === 충돌 대기 ===
			.Log(TEXT("Fireball: 충돌 대기 중..."))
			.WaitCollision(Spawned)                     // 충돌 시 Hit = 충돌 대상

			// === 충돌 처리 ===
			.Log(TEXT("Fireball: 충돌! 폭발 처리"))

			// 파이어볼 위치 저장 (폭발 위치)
			.GetPosition(R3, Spawned)                   // R3,R4,R5 = 폭발 위치

			// 파이어볼 제거
			.DestroyEntity(Spawned)

			// 직격 대상에게 100 피해
			.ApplyDamageConst(Hit, 100)
			.PlayVFXAttached(Hit, VFX_DirectHit)

			// 폭발 이펙트
			.PlayVFX(R3, VFX_FireballExplosion)
			.PlaySoundAtLocation(R3, Sound_Explosion)

			// === 범위 피해 (반경 300cm) ===
			.Log(TEXT("Fireball: 범위 피해 적용"))

			.ForEachInRadius(Hit, 300)                  // Hit 주변 300cm 내 적들
				.Move(Target, Iter)                     // Target = 현재 순회 대상
				.ApplyDamageConst(Target, 50)           // 50 피해
				.ApplyEffect(Target, Effect_Burn)
			.EndForEach()

			// 상체 애니메이션 초기화
			.StopAnim(Self, Anim_UpperBody_Cast_Fireball)

			.Log(TEXT("Fireball: 완료"))
			.Halt()
			.BuildAndRegister();
	}
}
