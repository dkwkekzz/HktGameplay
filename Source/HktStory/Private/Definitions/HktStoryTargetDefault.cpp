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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_AttackEngage,  "Story.Event.Attack.Engage",    "Attack engage story — range check + approach + attack (dispatch target).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_MoveToLocation, "Story.Event.Move.ToLocation", "Move to location story (dispatch target).");

	/**
	 * ================================================================
	 * Target Default Dispatcher
	 *
	 * 자연어로 읽으면:
	 * "타겟이 유효한 엔티티면 속성을 확인한다.
	 *  바닥 아이템이면 Pickup, NPC면 기본 공격, 그 외에는 이동을 디스패치한다."
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

			// NPC → 접근 공격 디스패치 (거리 검사 + 이동 + 공격)
			.Log(TEXT("TargetDefault: NPC → AttackEngage"))
			.DispatchEvent(Story_AttackEngage)
			.Halt()

		.Label(TEXT("dispatch_move"))
			// 이동 디스패치
			.Log(TEXT("TargetDefault: 이동"))
			.DispatchEvent(Story_MoveToLocation)
			.Halt()
		.BuildAndRegister();
	}
}
