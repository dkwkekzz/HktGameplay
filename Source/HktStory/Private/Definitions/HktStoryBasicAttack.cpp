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

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.Niagara.HitSpark", "Melee hit spark VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Swing, "Sound.Swing", "Melee swing sound.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/** 기본 공격 사거리 (cm) — AttackRange 프로퍼티가 0이면 이 값 사용 */
	static constexpr int32 DefaultAttackRange = 200;

	/** 접근 이동 Force */
	static constexpr int32 ChaseForce = 150;

	/**
	 * ================================================================
	 * Basic Attack Flow
	 *
	 * 자연어로 읽으면:
	 * "타겟 유효성을 확인한다.
	 *  공격 사거리를 계산하고, 사거리 밖이면 타겟에게 접근 이동 후 도착을 대기한다.
	 *  사거리 내이면 타겟을 바라보고, 공격 모션을 트리거한다 (fire-and-forget).
	 *  닿은 대상에게 데미지를 적용하고, 피격 위치에 이펙트를 생성한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_BasicAttack);
		B.CancelOnDuplicate()
		 .Log(TEXT("BasicAttack: 공격 시작"))

			// === 1. 타겟 유효성 검사 ===
			.LoadConst(R3, InvalidEntityId)
			.Move(R4, Target)
			.CmpEq(Flag, R4, R3)
			.JumpIf(Flag, TEXT("target_lost"))

			// === 2. 사거리 계산 ===
			.LoadStore(R0, PropertyId::AttackRange)
			.LoadConst(R1, 0)
			.CmpGt(Flag, R0, R1)
			.JumpIf(Flag, TEXT("range_ready"))
			.LoadConst(R0, DefaultAttackRange)
		.Label(TEXT("range_ready"))
			// R0 = AttackRange (이하 보존)

			// === 3. 사거리 판정 → 사거리 밖이면 접근 ===
			.GetDistance(R1, Self, Target)
			.CmpLe(Flag, R1, R0)
			.JumpIf(Flag, TEXT("in_range"))

			// 사거리 밖 → 타겟에게 접근
			.GetPosition(R2, Target)
			.MoveToward(Self, R2, ChaseForce)
			.WaitMoveEnd(Self)

			// 도착 후 사거리 재확인
			.GetDistance(R1, Self, Target)
			.CmpLe(Flag, R1, R0)
			.JumpIfNot(Flag, TEXT("miss"))

		.Label(TEXT("in_range"))
			// === 4. 타겟을 바라본다 ===
			.LookAt(Self, Target)

			// === 5. 공격 애니메이션 트리거 (fire-and-forget) + 스윙 사운드 ===
			.PlaySound(Sound_Swing);
		HktSnippetCombat::AnimTrigger(B, Self, Tag_Anim_Montage_Attack);

		B
			// === 6. 데미지 적용 ===
			.LoadStore(R0, PropertyId::AttackPower)
			.ApplyDamage(Target, R0)
			.PlaySound(Sound_Hit);

		// === 7. 피격 위치에 VFX ===
		B.GetPosition(R2, Target)
		 .PlayVFX(R2, VFX_HitSpark);

		// === 8. 피격 대상 히트리액션 (fire-and-forget) ===
		HktSnippetCombat::AnimTrigger(B, Target, Tag_Anim_Montage_HitReaction);

		B.Jump(TEXT("done"));

		// === 타겟 소실 ===
		B.Label(TEXT("target_lost"))
		 .StopMovement(Self)
		 .Log(TEXT("BasicAttack: 타겟 소실 — 중단"))
		 .Jump(TEXT("done"));

		// === 빗나감 ===
		B.Label(TEXT("miss"))
		 .Log(TEXT("BasicAttack: 사거리 밖 — 빗나감"));

		// === 후처리 (공통) ===
		// Note: NextActionFrame 갱신은 UseSkill에서 이미 수행됨 (CooldownUpdateConst)
		B.Label(TEXT("done"));

		B.Log(TEXT("BasicAttack: 완료"))
		 .Halt()
		.BuildAndRegister();
	}
}
