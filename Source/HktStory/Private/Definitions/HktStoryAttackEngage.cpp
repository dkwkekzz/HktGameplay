// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryAttackEngage
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_AttackEngage, "Story.Event.Attack.Engage", "Engage target — check range, approach if needed, then dispatch BasicAttack.");

	// Dispatch target
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_AttackBasic, "Story.Event.Attack.Basic", "Basic attack story (dispatch target).");

	/** 기본 공격 사거리 (cm) — AttackRange 프로퍼티가 0이면 이 값 사용 */
	static constexpr int32 DefaultAttackRange = 200;

	/** 접근 이동 Force (MoveTo와 동일) */
	static constexpr int32 ChaseForce = 150;

	/**
	 * ================================================================
	 * Attack Engage Flow
	 *
	 * 자연어로 읽으면:
	 * "타겟과의 거리를 측정한다. 공격 사거리 안이면 즉시 공격을 디스패치한다.
	 *  사거리 밖이면 타겟을 향해 이동하고, 매 프레임 거리를 재측정하여
	 *  사거리 안에 도달하면 이동을 정지하고 공격을 디스패치한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_AttackEngage)
			.CancelOnDuplicate()
			.Log(TEXT("AttackEngage: 접근 공격 시작"))

			// AttackRange 로드 (Self의 프로퍼티)
			.LoadStore(R0, PropertyId::AttackRange)
			.LoadConst(R1, 0)
			.CmpGt(Flag, R0, R1)
			.JumpIf(Flag, TEXT("range_loaded"))
			// AttackRange == 0 → 기본값 사용
			.LoadConst(R0, DefaultAttackRange)
		.Label(TEXT("range_loaded"))
			// R0 = AttackRange (이하 보존)

			// === 거리 검사 루프 ===
		.Label(TEXT("check_range"))
			// 타겟 유효성 검사 (타겟이 사라졌을 수 있음)
			.LoadConst(R3, InvalidEntityId)
			.Move(R4, Target)
			.CmpEq(Flag, R4, R3)
			.JumpIf(Flag, TEXT("target_lost"))

			// 거리 측정
			.GetDistance(R1, Self, Target)
			// R1 = 현재 거리, R0 = 공격 사거리
			.CmpLe(Flag, R1, R0)
			.JumpIf(Flag, TEXT("in_range"))

			// === 사거리 밖 → 타겟 방향으로 이동 ===
			.GetPosition(R2, Target)    // R2,R3,R4 = 타겟 위치
			.MoveToward(Self, R2, ChaseForce)
			.Yield(1)
			.Jump(TEXT("check_range"))

		.Label(TEXT("in_range"))
			// 사거리 안 → 이동 정지 후 공격 디스패치
			.StopMovement(Self)
			.Log(TEXT("AttackEngage: 사거리 도달 → BasicAttack 디스패치"))
			.DispatchEvent(Story_AttackBasic)
			.Halt()

		.Label(TEXT("target_lost"))
			.StopMovement(Self)
			.Log(TEXT("AttackEngage: 타겟 소실 — 중단"))
			.Halt()
		.BuildAndRegister();
	}
}
