// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktBagTypes.h"
#include "HktServerRuleInterfaces.h"
#include "HktWorldState.h"
#include "HktRuntimeDelegates.h"
#include "HktRuntimeTypes.h"

#include "HktBagComponent.generated.h"

/**
 * UHktBagComponent — 플레이어 가방 관리 컴포넌트 (IHktPlayerBag 구현)
 *
 * PlayerController에 부착. 서버에서 가방 상태를 관리하고,
 * Client RPC로 소유자 클라이언트에게만 가방 변경을 전달한다.
 *
 * 아키텍처:
 *   - 서버: ServerBagState에 아이템 저장/제거, Client_ReceiveBagUpdate RPC 전송
 *   - 클라: LocalBagState 캐시, FOnHktBagChanged 델리게이트 브로드캐스트
 *   - Entity ↔ Bag 전환은 ServerRule이 IHktPlayerBag 인터페이스를 통해 수행
 */
UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktBagComponent : public UActorComponent, public IHktPlayerBag
{
	GENERATED_BODY()

public:
	UHktBagComponent();

	// =================================================================
	// IHktPlayerBag 구현
	// =================================================================
	virtual const FHktBagState& GetBagState() const override { return ServerBagState; }
	virtual bool StoreToBag(const FHktBagItem& InItem, int32& OutBagSlot) override;
	virtual bool TakeFromBag(int32 BagSlot, FHktBagItem& OutItem) override;
	virtual void RestoreFromRecord(const TArray<FHktBagItem>& InBagItems, int32 InCapacity = 20) override;
	virtual TArray<FHktBagItem> ExportForRecord() const override { return ServerBagState.Items; }
	virtual void SendFullSync() override;

	// =================================================================
	// 서버 전용 API — 엔티티 레벨 조작 (ServerRule에서 호출)
	// =================================================================

	/**
	 * 엔티티의 아이템 프로퍼티를 스냅샷하여 가방에 저장.
	 * @param WS        현재 WorldState (아이템 프로퍼티 읽기)
	 * @param ItemEntity 저장할 아이템 엔티티
	 * @param OutBagSlot 할당된 가방 슬롯 (out)
	 * @return 성공시 true
	 */
	bool Server_StoreFromEntity(const FHktWorldState& WS, FHktEntityId ItemEntity, int32& OutBagSlot);

	/** 단일 아이템 변경 델타를 소유자 클라이언트에 전송 */
	void Server_SendDelta(EHktBagOp Op, const FHktBagItem& Item);

	// =================================================================
	// S2C RPC — 소유자 클라이언트에게만 전달
	// =================================================================

	UFUNCTION(Client, Reliable)
	void Client_ReceiveBagUpdate(const FHktRuntimeBagUpdate& Update);

	// =================================================================
	// 클라이언트 API
	// =================================================================

	/** 클라이언트 로컬 가방 상태 조회 */
	const FHktBagState& GetLocalBagState() const { return LocalBagState; }

	/** 가방 변경 델리게이트 (UI 구독용) */
	FOnHktBagChanged& OnBagChanged() { return BagChangedDelegate; }

private:
	/** 서버측 가방 상태 (서버에서만 수정) */
	FHktBagState ServerBagState;

	/** 클라이언트측 가방 캐시 (S2C RPC로 업데이트) */
	FHktBagState LocalBagState;

	/** 가방 변경 알림 델리게이트 */
	FOnHktBagChanged BagChangedDelegate;
};
