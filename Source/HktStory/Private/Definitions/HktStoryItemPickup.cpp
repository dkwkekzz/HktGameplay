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

namespace HktStoryItemPickup
{
	using namespace HktStoryTags;
	using namespace HktGameplayTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Pickup, "Story.Event.Item.Pickup", "Item pickup intent event.");

	/**
	 * ================================================================
	 * 아이템 줍기 Flow (Ground → Active)
	 *
	 * 자연어로 읽으면:
	 * "아이템이 Ground 상태이고 거리 3m 이내이며 빈 EquipIndex이 있으면
	 *  아이템을 즉시 활성화하여 장착하고, 스탯과 Stance를 적용한다."
	 *
	 * 클라이언트 인텐트 → 서버 fire.
	 * Self = 줍는 유닛, Target = 줍을 아이템 엔티티
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Event_Item_Pickup);
		B.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity) || !WS.IsValidEntity(E.TargetEntity))
					return false;

				// Ground 상태 확인
				if (WS.GetProperty(E.TargetEntity, PropertyId::ItemState) != 0)
					return false;

				// 거리 검증 (3m = 300cm)
				FIntVector SelfPos = WS.GetPosition(E.SourceEntity);
				FIntVector TargetPos = WS.GetPosition(E.TargetEntity);
				float DX = static_cast<float>(TargetPos.X - SelfPos.X);
				float DY = static_cast<float>(TargetPos.Y - SelfPos.Y);
				float DZ = static_cast<float>(TargetPos.Z - SelfPos.Z);
				if (DX * DX + DY * DY + DZ * DZ > 300.0f * 300.0f)
					return false;

				// 빈 EquipIndex 존재 여부 확인 (EquipSlot0~8 중 값==0인 슬롯)
				static constexpr uint16 SlotProps[] = {
					PropertyId::EquipSlot0, PropertyId::EquipSlot1, PropertyId::EquipSlot2,
					PropertyId::EquipSlot3, PropertyId::EquipSlot4, PropertyId::EquipSlot5,
					PropertyId::EquipSlot6, PropertyId::EquipSlot7, PropertyId::EquipSlot8,
				};
				for (uint16 Prop : SlotProps)
				{
					if (WS.GetProperty(E.SourceEntity, Prop) == 0)
						return true;
				}
				return false;
			});

		// Ground 상태 확인
		HktSnippetItem::ValidateItemState(B, Target, 0, TEXT("fail"));

		B	// 거리 검증
			.GetDistance(R0, Self, Target)
			.LoadConst(R1, 300)                                         // 3m = 300cm
			.CmpGt(Flag, R0, R1)
			.JumpIf(Flag, TEXT("fail"));

		// 빈 EquipIndex 탐색 (R3 = 빈 슬롯 인덱스, 없으면 fail)
		HktSnippetItem::FindEmptyEquipSlot(B, R3, TEXT("fail"));

		B	// 소유권 설정
			.SaveEntityProperty(Target, PropertyId::OwnerEntity, Self)
			.SetOwnerUid(Target)                                            // 계정 소유 설정

			// Active 상태로 전환 + EquipIndex 설정
			.SaveConstEntity(Target, PropertyId::ItemState, 2)              // Active
			.SaveEntityProperty(Target, PropertyId::EquipIndex, R3);

		// 캐릭터의 EquipSlot[R3] = 아이템 EntityId
		B.Move(R2, Target);                                                 // R2 = 아이템 EntityId
		HktSnippetItem::SaveItemToEquipSlot(B, R3, R2);

		// 아이템 스탯 + Stance를 캐릭터에 적용
		HktSnippetItem::ApplyItemStats(B, Target, Self);

		B	.Log(TEXT("Item picked up and activated"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("Item pickup failed — precondition violation"))
			.Fail()
		.BuildAndRegister();
	}
}
