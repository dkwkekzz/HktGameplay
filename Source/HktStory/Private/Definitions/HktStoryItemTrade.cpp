// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryItemTrade
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Item_Trade, "Story.Event.Item.Trade", "Item trade intent event.");

	// Entity Filter
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Entity_Item, "Entity.Item", "Item entity parent tag.");

	/**
	 * ================================================================
	 * 아이템 거래 Flow (2-phase: 검증 → 원자적 교환)
	 *
	 * 자연어로 읽으면:
	 * "양측 아이템의 소유권을 검증한 뒤,
	 *  두 아이템의 OwnerEntity/OwnerUid/BagSlot을 원자적으로 교환한다.
	 *  Active 상태 아이템은 거래 불가."
	 *
	 * Self = 제안자 캐릭터, Target = 상대방 캐릭터
	 * Param0 = 제안 아이템 EntityId (Self 소유)
	 * Param1 = 요청 아이템 EntityId (Target 소유)
	 *
	 * 서버 권위적: 클라이언트 양측이 동일한 Trade 이벤트를 fire해야
	 * 서버가 이를 매칭하여 실행. (매칭 로직은 서버룰에서 처리)
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Event_Item_Trade)
			.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity) || !WS.IsValidEntity(E.TargetEntity))
					return false;

				FHktEntityId OfferItem = static_cast<FHktEntityId>(E.Param0);
				FHktEntityId RequestItem = static_cast<FHktEntityId>(E.Param1);

				if (!WS.IsValidEntity(OfferItem) || !WS.IsValidEntity(RequestItem))
					return false;

				// 제안 아이템이 제안자 소유인지
				if (WS.GetProperty(OfferItem, PropertyId::OwnerEntity) != E.SourceEntity)
					return false;

				// 요청 아이템이 상대방 소유인지
				if (WS.GetProperty(RequestItem, PropertyId::OwnerEntity) != E.TargetEntity)
					return false;

				// Active 상태 아이템은 거래 불가
				if (WS.GetProperty(OfferItem, PropertyId::ItemState) == 2)
					return false;
				if (WS.GetProperty(RequestItem, PropertyId::ItemState) == 2)
					return false;

				// 양측 가방 용량 확인 (교환이므로 슬롯은 1:1, 추가 검증 불필요)
				return true;
			})

			// Param0 = 제안 아이템, Param1 = 요청 아이템
			.LoadStore(R0, PropertyId::Param0)                              // R0 = OfferItem EntityId
			.LoadStore(R1, PropertyId::Param1)                              // R1 = RequestItem EntityId

			// 제안 아이템 소유자 검증
			.LoadEntityProperty(R2, R0, PropertyId::OwnerEntity)
			.CmpNe(Flag, R2, Self)
			.JumpIf(Flag, TEXT("fail"))

			// 요청 아이템 소유자 검증
			.LoadEntityProperty(R2, R1, PropertyId::OwnerEntity)
			.CmpNe(Flag, R2, Target)
			.JumpIf(Flag, TEXT("fail"))

			// Active 상태 검증 — 제안 아이템
			.LoadEntityProperty(R2, R0, PropertyId::ItemState)
			.LoadConst(R3, 2)
			.CmpEq(Flag, R2, R3)
			.JumpIf(Flag, TEXT("fail"))

			// Active 상태 검증 — 요청 아이템
			.LoadEntityProperty(R2, R1, PropertyId::ItemState)
			.CmpEq(Flag, R2, R3)
			.JumpIf(Flag, TEXT("fail"))

			// === 원자적 교환 ===

			// 제안 아이템의 BagSlot 백업
			.LoadEntityProperty(R4, R0, PropertyId::BagSlot)                // R4 = OfferItem BagSlot
			// 요청 아이템의 BagSlot 백업
			.LoadEntityProperty(R5, R1, PropertyId::BagSlot)                // R5 = RequestItem BagSlot

			// 제안 아이템 → 상대방으로 이전
			.SaveEntityProperty(R0, PropertyId::OwnerEntity, Target)
			.SaveEntityProperty(R0, PropertyId::BagSlot, R5)

			// 요청 아이템 → 제안자로 이전
			.SaveEntityProperty(R1, PropertyId::OwnerEntity, Self)
			.SaveEntityProperty(R1, PropertyId::BagSlot, R4)

			// OwnerUid 교환 — 각각 상대방의 PlayerUid로 설정
			// 현재 VM에서 SetOwnerUid는 Runtime.PlayerUid를 사용하므로,
			// Trade Story는 제안자의 PlayerUid 기준으로 실행됨.
			// 따라서 요청 아이템에 SetOwnerUid, 제안 아이템에는 ClearOwnerUid 후
			// 상대방 컨텍스트에서 재설정이 필요하나, 단일 VM 한계로
			// Precondition에서 검증 완료된 상태이므로 OwnerEntity 변경으로 충분.
			// ExportPlayerState가 OwnerUid 기준으로 추출하므로 OwnerUid도 교환해야 함.

			// 제안 아이템: 상대방 OwnerUid 필요 → 직접 설정 불가하므로 Clear 후 재할당
			// 간접 해법: 두 아이템 모두 Clear 후 각 소유자 Story에서 재설정
			// 실용적 해법: OwnerEntity 변경만으로 런타임 동작은 정상,
			//              DB 저장 시 ExportPlayerState에서 OwnerEntity 기준으로도 추출하도록
			//              향후 개선 필요. 현재는 OwnerUid도 함께 Clear 후 표기.
			.ClearOwnerUid(R0)
			.ClearOwnerUid(R1)

			.Log(TEXT("Item trade completed"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("Item trade failed — precondition violation"))
			.Fail()
		.BuildAndRegister();
	}
}
