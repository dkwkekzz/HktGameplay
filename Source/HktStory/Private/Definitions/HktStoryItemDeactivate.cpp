// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "HktStoryTags.h"
#include "HktRuntimeTags.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetItem.h"

namespace HktStoryItemDeactivate
{
	using namespace HktStoryTags;
	using namespace HktGameplayTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Deactivate, "Story.Event.Item.Deactivate", "Item deactivate intent event.");

	/**
	 * ================================================================
	 * 아이템 비활성화 Flow (Active → InBag)
	 *
	 * 자연어로 읽으면:
	 * "활성화된 아이템을 가방으로 되돌린다.
	 *  ActionSlot을 해제하고, 같은 ActionSlot의 다른 활성 아이템이
	 *  없으면 Stance를 Unarmed로 복원한다."
	 *
	 * Self = 유닛, Target = 비활성화할 아이템(Active)
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Event_Item_Deactivate);
		B.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity) || !WS.IsValidEntity(E.TargetEntity))
					return false;
				if (WS.GetProperty(E.TargetEntity, PropertyId::ItemState) != 2)
					return false;
				if (WS.GetProperty(E.TargetEntity, PropertyId::OwnerEntity) != E.SourceEntity)
					return false;
				return true;
			});

		// Active 상태 확인
		HktSnippetItem::ValidateItemState(B, Target, 2, TEXT("fail"));

		// 소유자 확인
		HktSnippetItem::ValidateOwnership(B, Target, TEXT("fail"));

		// 비활성화 전 ActionSlot 값 보존 → 캐릭터의 ItemSlot[N] 클리어
		B.LoadEntityProperty(R2, Target, PropertyId::ActionSlot);           // R2 = ActionSlot

		HktSnippetItem::ClearItemSlot(B, R2);

			// InBag으로 전환 + 스탯 차감
		HktSnippetItem::DeactivateToBag(B, Target, Self);

		B	// 다른 활성 아이템이 있는지 확인하여 Stance 복원 결정
			.FindByOwner(Self, Entity_Item)
			.LoadConst(R4, 0)                                               // R4 = 활성 아이템 존재 플래그

		.Label(TEXT("check_loop"))
			.NextFound()
			.JumpIfNot(Flag, TEXT("check_done"))

			// 방금 비활성화한 아이템은 스킵 (이미 InBag으로 변경됨)
			.CmpEq(R5, Iter, Target)
			.JumpIf(R5, TEXT("check_loop"))

			// 이 아이템이 Active 상태인지 확인
			.LoadEntityProperty(R5, Iter, PropertyId::ItemState)
			.LoadConst(R6, 2)
			.CmpEq(R5, R5, R6)
			.JumpIfNot(R5, TEXT("check_loop"))

			// 활성 아이템 발견 — Stance를 그 아이템의 것으로 유지
			.LoadEntityProperty(R3, Iter, PropertyId::Stance)
			.SaveEntityProperty(Self, PropertyId::Stance, R3)
			.LoadConst(R4, 1)                                               // 활성 아이템 있음 표시
			.Jump(TEXT("done"))

		.Label(TEXT("check_done"))
			// R4 == 0이면 다른 활성 아이템 없음 — Unarmed로 복원
			.LoadConst(R5, 0)
			.CmpNe(Flag, R4, R5)
			.JumpIf(Flag, TEXT("done"))
			.SetStance(Self, HktStance::Unarmed)

		.Label(TEXT("done"))
			.Log(TEXT("Item deactivated"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("Item deactivate failed — precondition violation"))
			.Fail()
		.BuildAndRegister();
	}
}
