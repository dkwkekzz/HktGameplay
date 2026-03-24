// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktBagComponent.h"
#include "HktCoreProperties.h"
#include "HktCoreEventLog.h"
#include "HktRuntimeLog.h"

UHktBagComponent::UHktBagComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// ============================================================================
// IHktPlayerBag 구현
// ============================================================================

bool UHktBagComponent::StoreToBag(const FHktBagItem& InItem, int32& OutBagSlot)
{
	if (ServerBagState.IsFull())
	{
		UE_LOG(LogHktRuntime, Warning, TEXT("BagComponent::StoreToBag — bag is full"));
		return false;
	}

	if (!InItem.IsValid())
	{
		UE_LOG(LogHktRuntime, Warning, TEXT("BagComponent::StoreToBag — invalid item"));
		return false;
	}

	FHktBagItem ItemCopy = InItem;
	ItemCopy.BagSlot = ServerBagState.FindEmptySlot();
	if (ItemCopy.BagSlot < 0)
	{
		return false;
	}

	OutBagSlot = ItemCopy.BagSlot;
	ServerBagState.Items.Add(ItemCopy);

	HKT_EVENT_LOG(HktLogTags::Runtime_Server,
		FString::Printf(TEXT("BagStore: BagSlot=%d ItemId=%d"),
			ItemCopy.BagSlot, ItemCopy.ItemId));

	Server_SendDelta(EHktBagOp::Added, ItemCopy);
	return true;
}

bool UHktBagComponent::TakeFromBag(int32 BagSlot, FHktBagItem& OutItem)
{
	if (!ServerBagState.RemoveBySlot(BagSlot, OutItem))
	{
		UE_LOG(LogHktRuntime, Warning, TEXT("BagComponent::TakeFromBag — BagSlot=%d not found"), BagSlot);
		return false;
	}

	HKT_EVENT_LOG(HktLogTags::Runtime_Server,
		FString::Printf(TEXT("BagTake: BagSlot=%d ItemId=%d"),
			BagSlot, OutItem.ItemId));

	Server_SendDelta(EHktBagOp::Removed, OutItem);
	return true;
}

void UHktBagComponent::RestoreFromRecord(const TArray<FHktBagItem>& InBagItems, int32 InCapacity)
{
	ServerBagState.Items = InBagItems;
	ServerBagState.Capacity = InCapacity;

	HKT_EVENT_LOG(HktLogTags::Runtime_Server,
		FString::Printf(TEXT("BagRestoreFromRecord: %d items, capacity=%d"),
			InBagItems.Num(), InCapacity));
}

void UHktBagComponent::SendFullSync()
{
	FHktBagDelta Delta;
	Delta.Op = EHktBagOp::FullSync;
	Delta.FullState = ServerBagState;

	Client_ReceiveBagUpdate(FHktRuntimeBagUpdate(MoveTemp(Delta)));

	HKT_EVENT_LOG(HktLogTags::Runtime_Server,
		FString::Printf(TEXT("BagFullSync: %d items"), ServerBagState.GetItemCount()));
}

// ============================================================================
// 서버 전용 API — 엔티티 레벨 조작
// ============================================================================

bool UHktBagComponent::Server_StoreFromEntity(
	const FHktWorldState& WS, FHktEntityId ItemEntity, int32& OutBagSlot)
{
	if (!WS.IsValidEntity(ItemEntity))
	{
		UE_LOG(LogHktRuntime, Warning, TEXT("BagComponent::StoreFromEntity — invalid entity %d"), ItemEntity);
		return false;
	}

	// 엔티티 프로퍼티를 FHktBagItem으로 스냅샷
	FHktBagItem BagItem;
	BagItem.ItemId              = WS.GetProperty(ItemEntity, PropertyId::ItemId);
	BagItem.AttackPower         = WS.GetProperty(ItemEntity, PropertyId::AttackPower);
	BagItem.Defense             = WS.GetProperty(ItemEntity, PropertyId::Defense);
	BagItem.Stance              = WS.GetProperty(ItemEntity, PropertyId::Stance);
	BagItem.ItemSkillTag        = WS.GetProperty(ItemEntity, PropertyId::ItemSkillTag);
	BagItem.SkillCPCost         = WS.GetProperty(ItemEntity, PropertyId::SkillCPCost);
	BagItem.SkillTargetRequired = WS.GetProperty(ItemEntity, PropertyId::SkillTargetRequired);
	BagItem.RecoveryFrame       = WS.GetProperty(ItemEntity, PropertyId::RecoveryFrame);
	BagItem.EntitySpawnTag      = WS.GetProperty(ItemEntity, PropertyId::EntitySpawnTag);

	return StoreToBag(BagItem, OutBagSlot);
}

void UHktBagComponent::Server_SendDelta(EHktBagOp Op, const FHktBagItem& Item)
{
	FHktBagDelta Delta;
	Delta.Op = Op;
	Delta.Item = Item;

	Client_ReceiveBagUpdate(FHktRuntimeBagUpdate(MoveTemp(Delta)));
}

// ============================================================================
// S2C RPC — 클라이언트측 처리
// ============================================================================

void UHktBagComponent::Client_ReceiveBagUpdate_Implementation(const FHktRuntimeBagUpdate& Update)
{
	const FHktBagDelta& Delta = Update.Value;

	switch (Delta.Op)
	{
	case EHktBagOp::FullSync:
		LocalBagState = Delta.FullState;
		HKT_EVENT_LOG(HktLogTags::Runtime_Client,
			FString::Printf(TEXT("BagFullSync received: %d items"), LocalBagState.GetItemCount()));
		break;

	case EHktBagOp::Added:
		LocalBagState.AddItem(Delta.Item);
		HKT_EVENT_LOG(HktLogTags::Runtime_Client,
			FString::Printf(TEXT("BagAdded: BagSlot=%d ItemId=%d"), Delta.Item.BagSlot, Delta.Item.ItemId));
		break;

	case EHktBagOp::Removed:
		{
			FHktBagItem Removed;
			LocalBagState.RemoveBySlot(Delta.Item.BagSlot, Removed);
			HKT_EVENT_LOG(HktLogTags::Runtime_Client,
				FString::Printf(TEXT("BagRemoved: BagSlot=%d ItemId=%d"), Delta.Item.BagSlot, Delta.Item.ItemId));
		}
		break;
	}

	BagChangedDelegate.Broadcast(Delta);
}
