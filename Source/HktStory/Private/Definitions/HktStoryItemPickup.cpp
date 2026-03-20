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

namespace HktStoryItemPickup
{
	using namespace HktStoryTags;
	using namespace HktGameplayTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Pickup, "Story.Event.Item.Pickup", "Item pickup intent event.");

	/**
	 * ================================================================
	 * 아이템 줍기 Flow
	 *
	 * 자연어로 읽으면:
	 * "아이템이 Ground 상태이고 거리 3m 이내이며 가방에 빈 슬롯이 있으면
	 *  아이템을 가방의 첫 번째 빈 슬롯에 넣는다."
	 *
	 * 클라이언트 인텐트 → 서버 fire.
	 * Self = 줍는 유닛, Target = 줍을 아이템 엔티티
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Event_Item_Pickup)
			.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
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

				// 빈 슬롯 존재 여부 확인
				int32 BagCapacity = WS.GetProperty(E.SourceEntity, PropertyId::BagCapacity);
				TArray<bool> SlotOccupied;
				SlotOccupied.SetNumZeroed(BagCapacity);

				WS.ForEachEntity([&](FHktEntityId Id, int32 Slot)
				{
					if (WS.Get(Slot, PropertyId::OwnerEntity) == E.SourceEntity
						&& WS.GetTagsBySlot(Slot).HasTag(Entity_Item))
					{
						int32 BagSlot = WS.Get(Slot, PropertyId::BagSlot);
						if (BagSlot >= 0 && BagSlot < BagCapacity)
							SlotOccupied[BagSlot] = true;
					}
				});

				for (int32 i = 0; i < BagCapacity; ++i)
				{
					if (!SlotOccupied[i])
						return true;
				}
				return false;
			})

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

			// 빈 BagSlot 탐색 — 슬롯 0부터 순서대로 점유 여부를 확인
			.LoadEntityProperty(R1, Self, PropertyId::BagCapacity)
			.LoadConst(R3, 0)                                               // R3 = 후보 슬롯

		.Label(TEXT("find_slot_loop"))
			// R3 >= BagCapacity이면 가방 가득 참
			.CmpGe(Flag, R3, R1)
			.JumpIf(Flag, TEXT("fail"))

			// R3 슬롯이 점유되었는지 확인: 소유 아이템 재순회
			.LoadConst(R4, 0)                                               // R4 = 점유 플래그
			.FindByOwner(Self, Entity_Item)

		.Label(TEXT("slot_check_loop"))
			.NextFound()
			.JumpIfNot(Flag, TEXT("slot_check_done"))
			.LoadEntityProperty(R5, Iter, PropertyId::BagSlot)
			.CmpEq(R6, R5, R3)
			.JumpIfNot(R6, TEXT("slot_check_loop"))
			// 슬롯 R3이 점유됨
			.LoadConst(R4, 1)

		.Label(TEXT("slot_check_done"))
			// R4 == 0이면 R3가 빈 슬롯
			.LoadConst(R5, 0)
			.CmpEq(Flag, R4, R5)
			.JumpIf(Flag, TEXT("found_slot"))
			// 다음 슬롯 시도
			.AddImm(R3, R3, 1)
			.Jump(TEXT("find_slot_loop"))

		.Label(TEXT("found_slot"))
			// R3 = 빈 BagSlot, 아이템을 가방으로 이동
			.SaveEntityProperty(Target, PropertyId::OwnerEntity, Self)
			.SetOwnerUid(Target)                                            // 계정 소유 설정 (Gap 6)
			.SaveConstEntity(Target, PropertyId::ItemState, 1)              // InBag
			.SaveEntityProperty(Target, PropertyId::BagSlot, R3)

			.Log(TEXT("Item picked up"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("Item pickup failed — precondition violation"))
			.Fail()
		.BuildAndRegister();
	}
}
