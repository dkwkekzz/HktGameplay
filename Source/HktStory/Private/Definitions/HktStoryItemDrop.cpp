// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetItem.h"

namespace HktStoryItemDrop
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Drop, "Story.Event.Item.Drop", "Item drop intent event.");

	/**
	 * ================================================================
	 * 아이템 드랍 Flow (Active → Ground)
	 *
	 * 자연어로 읽으면:
	 * "내 소유인 활성 아이템을 월드에 내려놓는다.
	 *  캐릭터의 EquipSlot을 클리어하고, 스탯을 차감한 뒤
	 *  소유자를 해제하고 현재 위치에 드랍한다."
	 *
	 * Self = 유닛, Target = 드랍할 아이템
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Event_Item_Drop);
		B.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity) || !WS.IsValidEntity(E.TargetEntity))
					return false;
				if (WS.GetProperty(E.TargetEntity, PropertyId::OwnerEntity) != E.SourceEntity)
					return false;
				return true;
			});

		// 소유자 확인
		HktSnippetItem::ValidateOwnership(B, Target, TEXT("fail"));

		// Active 상태였으면 캐릭터의 EquipSlot 클리어 + 스탯 차감
		B.LoadEntityProperty(R0, Target, PropertyId::ItemState)
		 .LoadConst(R1, 2)                                                 // Active = 2
		 .CmpNe(Flag, R0, R1)
		 .JumpIf(Flag, TEXT("drop_exec"));

		// EquipIndex 보존 → 캐릭터의 EquipSlot[N] 클리어
		B.LoadEntityProperty(R2, Target, PropertyId::EquipIndex);
		HktSnippetItem::ClearEquipSlot(B, R2);

		// Active 아이템 스탯 차감
		HktSnippetItem::RemoveItemStats(B, Target, Self);

		B.Label(TEXT("drop_exec"))
			// Ground로 전환
			.SaveConstEntity(Target, PropertyId::ItemState, 0)                // Ground
			.SaveConstEntity(Target, PropertyId::OwnerEntity, 0)              // 소유자 해제
			.ClearOwnerUid(Target)                                            // 계정 소유 해제
			.SaveConstEntity(Target, PropertyId::EquipIndex, -1)              // 액션 해제

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
