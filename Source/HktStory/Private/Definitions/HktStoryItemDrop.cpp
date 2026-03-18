// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryItemDrop
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Drop, "Event.Item.Drop", "Item drop intent event.");

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
			// 소유자 확인
			.LoadEntityProperty(R0, Target, PropertyId::OwnerEntity)
			.CmpNe(Flag, R0, Self)
			.JumpIf(Flag, TEXT("fail"))

			// Ground로 전환
			.SaveConst(Target, PropertyId::ItemState, 0)                // Ground
			.SaveConst(Target, PropertyId::OwnerEntity, 0)              // 소유자 해제
			.SaveConst(Target, PropertyId::BagSlot, 0)                  // 슬롯 초기화
			.SaveConst(Target, PropertyId::ActionSlot, -1)              // 액션 해제

			// 유닛 위치에 드랍
			.GetPosition(R3, Self)
			.SetPosition(Target, R3)

			.Log(TEXT("Item dropped"))
			.Halt()

		.Label(TEXT("fail"))
			.Halt()
		.BuildAndRegister();
	}
}
