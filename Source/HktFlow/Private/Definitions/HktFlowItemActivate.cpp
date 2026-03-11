// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktCoreProperties.h"
#include "HktFlowRegistry.h"
#include "NativeGameplayTags.h"

namespace HktFlowItemActivate
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Activate, "Event.Item.Activate", "Item activate intent event.");

	/**
	 * ================================================================
	 * 아이템 액션 등록 Flow (Active 상태에서 ActionSlot 설정)
	 *
	 * 자연어로 읽으면:
	 * "장착된 아이템을 액션 슬롯에 등록하여 활성화한다."
	 *
	 * Self = 유닛, Target = 활성화할 아이템(Active), Param0 = ActionSlot
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;

		Flow(Event_Item_Activate)
			// Active 상태 확인
			.LoadEntityProperty(R0, Target, PropertyId::ItemState)
			.LoadConst(R1, 2)                                           // Active = 2
			.CmpNe(Flag, R0, R1)
			.JumpIf(Flag, TEXT("fail"))

			// 소유자 확인
			.LoadEntityProperty(R0, Target, PropertyId::OwnerEntity)
			.CmpNe(Flag, R0, Self)
			.JumpIf(Flag, TEXT("fail"))

			// ActionSlot 설정
			.LoadStore(R2, PropertyId::Param0)                          // ActionSlot from event
			.SaveEntityProperty(Target, PropertyId::ActionSlot, R2)

			.Log(TEXT("Item activated"))
			.Halt()

		.Label(TEXT("fail"))
			.Halt()
		.BuildAndRegister();
	}
}
