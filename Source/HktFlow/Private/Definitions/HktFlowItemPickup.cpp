// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktCoreProperties.h"
#include "HktFlowRegistry.h"
#include "NativeGameplayTags.h"

namespace HktFlowItemPickup
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Pickup, "Event.Item.Pickup", "Item pickup intent event.");

	/**
	 * ================================================================
	 * 아이템 줍기 Flow
	 *
	 * 자연어로 읽으면:
	 * "아이템이 Ground 상태이고 거리 3m 이내이며 가방에 빈 공간이 있으면
	 *  아이템을 가방에 넣는다."
	 *
	 * 클라이언트 인텐트 → 서버 fire.
	 * Self = 줍는 유닛, Target = 줍을 아이템 엔티티
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;

		Flow(Event_Item_Pickup)
			// Ground 상태 확인
			.LoadEntityProperty(R0, Target, PropertyId::ItemState)
			.LoadConst(R1, 0)                                           // Ground = 0
			.CmpNe(Flag, R0, R1)
			.JumpIf(Flag, TEXT("fail"))

			// 거리 검증
			.GetDistance(R0, Self, Target)
			.LoadConst(R1, 300)                                         // 3m = 300cm
			.CmpGt(Flag, R0, R1)
			.JumpIf(Flag, TEXT("fail"))

			// 가방 공간 확인
			.CountByOwner(R0, Self, HktType::Equipment)
			.LoadConst(R1, 20)                                          // 가방 용량
			.CmpGe(Flag, R0, R1)
			.JumpIf(Flag, TEXT("fail"))

			// 아이템을 가방으로 이동
			.Move(R3, R0)                                               // BagSlot = 현재 카운트
			.LoadEntityProperty(R4, Self, PropertyId::EntityType)       // Self의 EntityId를 직접 사용
			.SaveEntityProperty(Target, PropertyId::OwnerEntity, Self)
			.LoadConst(R2, 1)
			.SaveEntityProperty(Target, PropertyId::ItemState, R2)      // InBag
			.SaveEntityProperty(Target, PropertyId::BagSlot, R3)

			.Log(TEXT("Item picked up"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("Item pickup failed"))
			.Halt()
		.BuildAndRegister();
	}
}
