// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryTargetDefault
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_TargetDefault, "Story.Event.Target.Default", "Default target action dispatcher — branches by target type.");

	// Dispatch targets
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_ItemPickup,    "Story.Event.Item.Pickup",      "Item pickup story (dispatch target).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_UseSkill,      "Story.Event.Combat.UseSkill",  "Unified skill pipeline (dispatch target).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_MoveToLocation, "Story.Event.Move.ToLocation", "Move to location story (dispatch target).");

	/** 기본 공격 사거리 (cm) — AttackRange 프로퍼티가 0이면 이 값 사용 */
	static constexpr int32 DefaultAttackRange = 200;

	/** 접근 이동 Force */
	static constexpr int32 ChaseForce = 150;

	/**
	 * ================================================================
	 * Target Default Dispatcher
	 *
	 * 자연어로 읽으면:
	 * "타겟이 유효한 엔티티면 속성을 확인한다.
	 *  바닥 아이템이면 Pickup을 디스패치한다.
	 *  NPC면 사거리 검증 후 접근하고, 쿨타임을 확인한 뒤 UseSkill을 디스패치한다.
	 *  그 외에는 이동을 디스패치한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_TargetDefault)
			.CancelOnDuplicate()
			.Log(TEXT("TargetDefault: 타겟 유형 판별 시작"))

			// Target이 Invalid이면 이동
			.LoadConst(R0, InvalidEntityId)
			.Move(R1, Target)
			.CmpEq(Flag, R1, R0)
			.JumpIf(Flag, TEXT("dispatch_move"))

			// Target의 ItemId 확인
			.LoadStoreEntity(R0, Target, PropertyId::ItemId)
			.LoadConst(R1, 0)
			.CmpGt(Flag, R0, R1)
			.JumpIfNot(Flag, TEXT("check_npc"))

			// ItemState == 0 (Ground) 확인
			.LoadStoreEntity(R0, Target, PropertyId::ItemState)
			.CmpEq(Flag, R0, R1)
			.JumpIfNot(Flag, TEXT("dispatch_move"))

			// 바닥 아이템 → Pickup 디스패치
			.Log(TEXT("TargetDefault: 아이템 → Pickup"))
			.DispatchEvent(Story_ItemPickup)
			.Halt()

		.Label(TEXT("check_npc"))
			// IsNPC > 0 확인
			.LoadStoreEntity(R0, Target, PropertyId::IsNPC)
			.LoadConst(R1, 0)
			.CmpGt(Flag, R0, R1)
			.JumpIfNot(Flag, TEXT("dispatch_move"))

			// === NPC: 거리 검증 + 접근 + 쿨타임 ===

			// 사거리 로드
			.LoadStore(R0, PropertyId::AttackRange)
			.LoadConst(R1, 0)
			.CmpGt(Flag, R0, R1)
			.JumpIf(Flag, TEXT("npc_range_ok"))
			.LoadConst(R0, DefaultAttackRange)
		.Label(TEXT("npc_range_ok"))
			// R0 = AttackRange

			// 거리 측정
			.GetDistance(R1, Self, Target)
			.CmpLe(Flag, R1, R0)
			.JumpIf(Flag, TEXT("npc_check_cd"))

			// 사거리 밖 → 타겟에게 접근 (R0 = AttackRange 유지)
		.Label(TEXT("npc_chase_loop"))
			.GetPosition(R2, Target)
			.MoveToward(Self, R2, ChaseForce)
			.Yield(1)
			.GetDistance(R1, Self, Target)
			.CmpLe(Flag, R1, R0)
			.JumpIfNot(Flag, TEXT("npc_chase_loop"))
			.StopMovement(Self)

		.Label(TEXT("npc_check_cd"))
			// 쿨타임 검증
			.GetWorldTime(R2)
			.LoadStore(R3, PropertyId::NextActionFrame)
			.CmpGe(Flag, R2, R3)
			.JumpIfNot(Flag, TEXT("npc_done"))

			// 검증 통과 → UseSkill 디스패치
			.Log(TEXT("TargetDefault: NPC → UseSkill"))
			.DispatchEvent(Story_UseSkill)

		.Label(TEXT("npc_done"))
			.Halt()

		.Label(TEXT("dispatch_move"))
			// 이동 디스패치
			.Log(TEXT("TargetDefault: 이동"))
			.DispatchEvent(Story_MoveToLocation)
			.Halt()
		.BuildAndRegister();
	}
}
