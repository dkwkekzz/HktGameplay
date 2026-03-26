// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetCombat.h"

namespace HktStoryBasicAttack
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_BasicAttack, "Story.Event.Attack.Basic", "Basic attack ability flow.");

	// 공격 애니메이션 (트리거 — fire-and-forget)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage trigger tag.");

	// 피격 애니메이션 (트리거 — fire-and-forget)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_HitReaction, "Anim.Montage.HitReaction", "Hit reaction montage trigger tag.");

	// 히트 영역 엔티티 (공격자 위치에 스폰, CollisionRadius로 충돌 감지)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_HitArea, "Entity.Area.HitTest", "Hit test area entity spawned at attacker position.");

	// 히트 이펙트 엔티티
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_HitEffect, "Entity.Effect.Hit", "Hit effect entity spawned at impact point.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.Niagara.HitSpark", "Melee hit spark VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Swing, "Sound.Swing", "Melee swing sound.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/** 기본 공격 후딜레이 (프레임) — AttackSpeed로 나뉨 */
	static constexpr int32 BasicAttackRecoveryFrame = 30;

	/** 기본 공격 사거리 (cm) — AttackRange 프로퍼티가 0이면 이 값 사용 */
	static constexpr int32 DefaultAttackRange = 200;

	/**
	 * ================================================================
	 * Basic Attack Flow (충돌 기반 히트 판정)
	 *
	 * 자연어로 읽으면:
	 * "타겟을 바라보고, 공격 모션을 트리거한다 (fire-and-forget).
	 *  공격자 위치에 히트테스트 영역을 절차적으로 생성하고,
	 *  충돌이 감지될 때까지 대기한다.
	 *  닿은 대상에게 데미지를 적용하고, 부딧친 위치에 히트 이펙트를 생성한다."
	 *
	 * 쿨타임/사거리 검증은 AttackEngage에서 이미 완료됨.
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_BasicAttack);
		B.Log(TEXT("BasicAttack: 공격 시작"))

			// === 1. 타겟을 바라본다 ===
			.LookAt(Self, Target)

			// === 2. 공격 애니메이션 트리거 (fire-and-forget) + 스윙 사운드 ===
			.AddTag(Self, Tag_Anim_Montage_Attack)
			.PlaySound(Sound_Swing)

			// === 3. 히트테스트 영역 절차적 생성 ===
			.LoadStore(R0, PropertyId::AttackRange)
			.LoadConst(R1, 0)
			.CmpGt(Flag, R0, R1)
			.JumpIf(Flag, TEXT("range_ready"))
			.LoadConst(R0, DefaultAttackRange)
		.Label(TEXT("range_ready"));

		// 히트 영역 엔티티 스폰
		B.SpawnEntity(Entity_HitArea)
		 .GetPosition(R2, Self)
		 .SetPosition(Spawned, R2)
		 .SaveStoreEntity(Spawned, PropertyId::CollisionRadius, R0);

		// === 4. 충돌 대기 (Self-collision 필터) ===
		B.Label(TEXT("wait_hit"))
		 .WaitCollision(Spawned)
		 .Move(R3, Hit)
		 .Move(R4, Self)
		 .CmpEq(Flag, R3, R4)
		 .JumpIf(Flag, TEXT("wait_hit"));

		// === 5. 히트 영역 제거 ===
		B.DestroyEntity(Spawned);

		// === 6. 데미지 적용 ===
		B.LoadStore(R0, PropertyId::AttackPower)
		 .ApplyDamage(Hit, R0)
		 .PlaySound(Sound_Hit);

		// === 7. 피격 위치에 히트 이펙트 생성 ===
		B.SpawnEntity(Entity_HitEffect)
		 .GetPosition(R2, Hit)
		 .SetPosition(Spawned, R2)
		 .PlayVFXAttached(Spawned, VFX_HitSpark);

		// === 8. 피격 대상 히트리액션 (fire-and-forget) ===
		B.AddTag(Hit, Tag_Anim_Montage_HitReaction);

		// === 9. 히트 이펙트 제거 ===
		B.DestroyEntity(Spawned);

		// === 10. 쿨타임 갱신 ===
		HktSnippetCombat::CooldownUpdateConst(B, BasicAttackRecoveryFrame);

		B.Log(TEXT("BasicAttack: 완료"))
		 .Halt()
		.BuildAndRegister();
	}
}
