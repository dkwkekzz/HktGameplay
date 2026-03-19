// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryItemDrop
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Drop, "Story.Event.Item.Drop", "Item drop intent event.");

	/**
	 * ================================================================
	 * 아이템 드랍 Flow (Active/InBag → Ground)
	 *
	 * 자연어로 읽으면:
	 * "내 소유인 아이템을 월드에 내려놓는다.
	 *  소유자를 해제하고 현재 위치에 드랍한다."
	 *
	 * Self = 유닛, Target = 드랍할 아이템
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Event_Item_Drop)
			.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity) || !WS.IsValidEntity(E.TargetEntity))
					return false;

				// 소유자 확인
				if (WS.GetProperty(E.TargetEntity, PropertyId::OwnerEntity) != E.SourceEntity)
					return false;

				return true;
			})

			// 소유자 확인
			.LoadEntityProperty(R0, Target, PropertyId::OwnerEntity)
			.CmpNe(Flag, R0, Self)
			.JumpIf(Flag, TEXT("fail"))

			// Active 상태였으면 캐릭터에서 스탯 차감 (Gap 4)
			.LoadEntityProperty(R0, Target, PropertyId::ItemState)
			.LoadConst(R1, 2)                                                 // Active = 2
			.CmpNe(Flag, R0, R1)
			.JumpIf(Flag, TEXT("drop_exec"))

			// Active 아이템 스탯 차감
			.LoadEntityProperty(R0, Target, PropertyId::AttackPower)
			.LoadEntityProperty(R1, Self, PropertyId::AttackPower)
			.Sub(R1, R1, R0)
			.SaveEntityProperty(Self, PropertyId::AttackPower, R1)
			.LoadEntityProperty(R0, Target, PropertyId::Defense)
			.LoadEntityProperty(R1, Self, PropertyId::Defense)
			.Sub(R1, R1, R0)
			.SaveEntityProperty(Self, PropertyId::Defense, R1)

		.Label(TEXT("drop_exec"))
			// Ground로 전환
			.SaveConstEntity(Target, PropertyId::ItemState, 0)                // Ground
			.SaveConstEntity(Target, PropertyId::OwnerEntity, 0)              // 소유자 해제
			.ClearOwnerUid(Target)                                            // 계정 소유 해제 (Gap 6)
			.SaveConstEntity(Target, PropertyId::BagSlot, 0)                  // 슬롯 초기화
			.SaveConstEntity(Target, PropertyId::ActionSlot, -1)              // 액션 해제

			// 유닛 위치에 드랍
			.GetPosition(R3, Self)
			.SetPosition(Target, R3)

			.Log(TEXT("Item dropped"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("Item drop failed — precondition violation"))
			.Fail()
		.BuildAndRegister();
	}
}
