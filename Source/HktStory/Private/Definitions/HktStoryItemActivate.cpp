// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryItemActivate
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Activate, "Story.Event.Item.Activate", "Item activate intent event.");

	// Entity Filter
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Entity_Item, "Entity.Item", "Item entity parent tag.");

	/**
	 * ================================================================
	 * 아이템 활성화 Flow (InBag → Active + ActionSlot + Stance)
	 *
	 * 자연어로 읽으면:
	 * "가방의 아이템을 활성화하여 액션 슬롯에 등록하고,
	 *  아이템의 Stance를 캐릭터에 적용한다.
	 *  같은 ActionSlot에 이미 다른 아이템이 있으면 자동으로 비활성화한다."
	 *
	 * Self = 유닛, Target = 활성화할 아이템(InBag), Param0 = ActionSlot
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Event_Item_Activate)
			.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity) || !WS.IsValidEntity(E.TargetEntity))
					return false;

				// InBag 상태 확인
				if (WS.GetProperty(E.TargetEntity, PropertyId::ItemState) != 1)
					return false;

				// 소유자 확인
				if (WS.GetProperty(E.TargetEntity, PropertyId::OwnerEntity) != E.SourceEntity)
					return false;

				return true;
			})

			// InBag 상태 확인
			.LoadEntityProperty(R0, Target, PropertyId::ItemState)
			.LoadConst(R1, 1)                                           // InBag = 1
			.CmpNe(Flag, R0, R1)
			.JumpIf(Flag, TEXT("fail"))

			// 소유자 확인
			.LoadEntityProperty(R0, Target, PropertyId::OwnerEntity)
			.CmpNe(Flag, R0, Self)
			.JumpIf(Flag, TEXT("fail"))

			// 요청된 ActionSlot 로드
			.LoadStore(R2, PropertyId::Param0)                              // R2 = ActionSlot from event

			// 동일 ActionSlot에 이미 활성된 아이템이 있으면 자동 비활성화
			.FindByOwner(Self, Tag_Entity_Item)

		.Label(TEXT("evict_loop"))
			.NextFound()
			.JumpIfNot(Flag, TEXT("evict_done"))

			// Active(State==2) 상태인지 확인
			.LoadEntityProperty(R4, Iter, PropertyId::ItemState)
			.LoadConst(R5, 2)
			.CmpNe(R6, R4, R5)
			.JumpIf(R6, TEXT("evict_loop"))

			// 같은 ActionSlot인지 확인
			.LoadEntityProperty(R4, Iter, PropertyId::ActionSlot)
			.CmpNe(R6, R4, R2)
			.JumpIf(R6, TEXT("evict_loop"))

			// 충돌 발견 — 기존 아이템을 InBag으로 전환
			.SaveConstEntity(Iter, PropertyId::ItemState, 1)                // InBag
			.SaveConstEntity(Iter, PropertyId::ActionSlot, -1)              // 액션 해제
			.Log(TEXT("Evicted existing item from ActionSlot"))

		.Label(TEXT("evict_done"))
			// Active 상태로 전환 + ActionSlot 설정
			.SaveConstEntity(Target, PropertyId::ItemState, 2)              // Active
			.SaveEntityProperty(Target, PropertyId::ActionSlot, R2)

			// 아이템의 Stance를 캐릭터에 적용
			.LoadEntityProperty(R3, Target, PropertyId::Stance)
			.SaveEntityProperty(Self, PropertyId::Stance, R3)

			.Log(TEXT("Item activated"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("Item activate failed — precondition violation"))
			.Fail()
		.BuildAndRegister();
	}
}
