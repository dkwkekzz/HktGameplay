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

	// Hit Story (dispatch target)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_HitBasic, "Story.Event.Combat.Hit.Basic", "Basic attack hit story — damage + reaction.");

	// 공격 애니메이션 (트리거 — fire-and-forget)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage trigger tag.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Swing, "Sound.Swing", "Melee swing sound.");

	/** 기본 공격 사거리 (cm) — 히트테스트 반경으로 사용 */
	static constexpr int32 DefaultAttackRange = 200;

	/**
	 * ================================================================
	 * Basic Attack Flow
	 *
	 * 자연어로 읽으면:
	 * "타겟을 바라보고 공격 애니메이션을 재생한다.
	 *  공격 사거리 내 히트테스트 영역을 생성하여 적을 검색한다.
	 *  맞은 대상에게 히트 스토리를 디스패치한다."
	 *
	 * 사거리 검증과 접근은 TargetDefault에서 완료됨.
	 * 쿨타임 검증과 갱신은 UseSkill에서 수행됨.
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_BasicAttack);
		B.Log(TEXT("BasicAttack: 공격 시작"))

			// === 1. 타겟을 바라본다 ===
			.LookAt(Self, Target)

			// === 2. 공격 애니메이션 트리거 + 스윙 사운드 ===
			.PlaySound(Sound_Swing);
		HktSnippetCombat::AnimTrigger(B, Self, Tag_Anim_Montage_Attack);

		B	// === 3. 히트테스트 준비 ===
			.LoadStore(R5, PropertyId::Team)              // R5 = Self.Team (루프 내 보존)

			// === 4. 히트테스트 영역 — 공간 쿼리만 수행, 물리 영향 없음 ===
			.ForEachInRadius(Self, DefaultAttackRange)
				// Self 제외
				.Move(R6, Iter)
				.Move(R7, Self)
				.CmpEq(Flag, R6, R7)
				.JumpIf(Flag, TEXT("hit_skip"))

				// 같은 팀 제외
				.LoadEntityProperty(R6, Iter, PropertyId::Team)
				.CmpEq(Flag, R6, R5)
				.JumpIf(Flag, TEXT("hit_skip"))

				// Team 0 (비전투 엔티티) 제외
				.LoadConst(R7, 0)
				.CmpEq(Flag, R6, R7)
				.JumpIf(Flag, TEXT("hit_skip"))

				// === Hit! 대상에게 히트 스토리 디스패치 ===
				.DispatchEventTo(Story_HitBasic, Iter)

			.Label(TEXT("hit_skip"))
			.EndForEach()

			.Log(TEXT("BasicAttack: 완료"))
			.Halt()
		.BuildAndRegister();
	}
}
