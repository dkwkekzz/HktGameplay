// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktCoreProperties.h"
#include "HktFlowRegistry.h"
#include "NativeGameplayTags.h"

namespace HktFlowItemEquip
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Equip, "Event.Item.Equip", "Item equip intent event.");

	/**
	 * ================================================================
	 * 아이템 장착 Flow (InBag → Active)
	 *
	 * 자연어로 읽으면:
	 * "아이템이 InBag 상태이고 내 소유이면 Active(장착) 상태로 전환한다."
	 *
	 * Self = 유닛, Target = 장착할 아이템(InBag)
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;

		Flow(Event_Item_Equip)
			// InBag 상태 확인
			.LoadEntityProperty(R0, Target, PropertyId::ItemState)
			.LoadConst(R1, 1)                                           // InBag = 1
			.CmpNe(Flag, R0, R1)
			.JumpIf(Flag, TEXT("fail"))

			// 소유자 확인
			.LoadEntityProperty(R0, Target, PropertyId::OwnerEntity)
			.CmpNe(Flag, R0, Self)
			.JumpIf(Flag, TEXT("fail"))

			// Active로 전환
			.LoadConst(R2, 2)
			.SaveEntityProperty(Target, PropertyId::ItemState, R2)      // Active

			.Log(TEXT("Item equipped"))
			.Halt()

		.Label(TEXT("fail"))
			.Halt()
		.BuildAndRegister();
	}
}
