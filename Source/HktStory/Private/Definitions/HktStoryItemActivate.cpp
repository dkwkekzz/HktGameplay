// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryEventParams.h"
#include "HktStoryRegistry.h"
#include "HktStoryTags.h"
#include "HktRuntimeTags.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetItem.h"

namespace HktStoryItemActivate
{
	using namespace HktStoryTags;
	using namespace HktGameplayTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Activate, "Story.Event.Item.Activate", "Item activate intent event.");

	/**
	 * ================================================================
	 * 아이템 활성화 Flow (InBag → Active + EquipIndex + Stance)
	 *
	 * 자연어로 읽으면:
	 * "가방의 아이템을 활성화하여 액션 슬롯에 등록하고,
	 *  아이템의 Stance를 캐릭터에 적용한다.
	 *  같은 EquipIndex에 이미 다른 아이템이 있으면 자동으로 비활성화한다."
	 *
	 * Self = 유닛, Target = 활성화할 아이템(InBag), Param0 = EquipIndex
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Event_Item_Activate);
		B.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity) || !WS.IsValidEntity(E.TargetEntity))
					return false;
				if (WS.GetProperty(E.TargetEntity, PropertyId::ItemState) != 1)
					return false;
				if (WS.GetProperty(E.TargetEntity, PropertyId::OwnerEntity) != E.SourceEntity)
					return false;
				return true;
			});

		// InBag 상태 확인
		HktSnippetItem::ValidateItemState(B, Target, 1, TEXT("fail"));

		// 소유자 확인
		HktSnippetItem::ValidateOwnership(B, Target, TEXT("fail"));

		B	// 요청된 EquipIndex 로드
			.LoadStore(R2, ItemActivateParams::EquipIndex)                  // R2 = EquipIndex from event

			// 동일 EquipIndex에 이미 활성된 아이템이 있으면 자동 비활성화
			.FindByOwner(Self, Entity_Item)

		.Label(TEXT("evict_loop"))
			.NextFound()
			.JumpIfNot(Flag, TEXT("evict_done"))

			// Active(State==2) 상태인지 확인
			.LoadEntityProperty(R4, Iter, PropertyId::ItemState)
			.LoadConst(R5, 2)
			.CmpNe(R6, R4, R5)
			.JumpIf(R6, TEXT("evict_loop"))

			// 같은 EquipIndex인지 확인
			.LoadEntityProperty(R4, Iter, PropertyId::EquipIndex)
			.CmpNe(R6, R4, R2)
			.JumpIf(R6, TEXT("evict_loop"))

			;

		// 충돌 발견 — 기존 아이템을 InBag으로 전환 + 스탯 차감
		HktSnippetItem::DeactivateToBag(B, Iter, Self);

		B	.Log(TEXT("Evicted existing item from EquipIndex"))
			.Jump(TEXT("evict_loop"))                                       // 계속 순회 (비정상 중복 대비)

		.Label(TEXT("evict_done"));

		// Active 상태로 전환 + EquipIndex 등록 + 스탯 적용
		HktSnippetItem::ActivateInSlot(B, Target, R2, Self);

		B	.Log(TEXT("Item activated"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("Item activate failed — precondition violation"))
			.Fail()
		.BuildAndRegister();
	}
}
