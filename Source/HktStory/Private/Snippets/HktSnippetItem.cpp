// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Snippets/HktSnippetItem.h"
#include "HktCoreProperties.h"

static constexpr uint16 ItemSlotProperties[] =
{
	PropertyId::ItemSlot0, PropertyId::ItemSlot1, PropertyId::ItemSlot2,
	PropertyId::ItemSlot3, PropertyId::ItemSlot4, PropertyId::ItemSlot5,
	PropertyId::ItemSlot6, PropertyId::ItemSlot7, PropertyId::ItemSlot8,
};

static constexpr int32 NumItemSlots = UE_ARRAY_COUNT(ItemSlotProperties);

FHktStoryBuilder& HktSnippetItem::LoadItemFromSlot(
	FHktStoryBuilder& B,
	RegisterIndex DstReg,
	const FString& FailLabel)
{
	using namespace Reg;

	FString P = B.MakeInternalLabel(TEXT("slot"));
	FString DoneLabel = P + TEXT("_done");

	// Param1에서 슬롯 인덱스 로드
	B.LoadStore(R0, PropertyId::Param1);                                    // R0 = 슬롯 인덱스

	// 디스패치: 각 슬롯 인덱스에 대해 비교 + 점프
	for (int32 i = 0; i < NumItemSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_ld%d"), *P, i);
		B.LoadConst(R1, i).CmpEq(Flag, R0, R1).JumpIf(Flag, BranchLabel);
	}
	B.Jump(FailLabel);                                                      // 유효하지 않은 슬롯

	// 로드 타겟
	for (int32 i = 0; i < NumItemSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_ld%d"), *P, i);
		B.Label(BranchLabel).LoadStore(DstReg, ItemSlotProperties[i]).Jump(DoneLabel);
	}

	B.Label(DoneLabel);

	// 유효성 검증: 아이템 엔티티 != 0
	B.LoadConst(R3, 0)
	 .CmpEq(Flag, DstReg, R3)
	 .JumpIf(Flag, FailLabel);

	return B;
}

FHktStoryBuilder& HktSnippetItem::SaveItemToSlot(
	FHktStoryBuilder& B,
	RegisterIndex SlotIndexReg,
	RegisterIndex ValueReg)
{
	using namespace Reg;

	FString P = B.MakeInternalLabel(TEXT("sslot"));
	FString DoneLabel = P + TEXT("_done");

	for (int32 i = 0; i < NumItemSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_s%d"), *P, i);
		B.LoadConst(R4, i).CmpEq(Flag, SlotIndexReg, R4).JumpIf(Flag, BranchLabel);
	}
	B.Jump(DoneLabel);

	for (int32 i = 0; i < NumItemSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_s%d"), *P, i);
		B.Label(BranchLabel).SaveEntityProperty(Self, ItemSlotProperties[i], ValueReg).Jump(DoneLabel);
	}

	B.Label(DoneLabel);

	return B;
}

FHktStoryBuilder& HktSnippetItem::ClearItemSlot(
	FHktStoryBuilder& B,
	RegisterIndex SlotIndexReg)
{
	using namespace Reg;

	FString P = B.MakeInternalLabel(TEXT("cslot"));
	FString DoneLabel = P + TEXT("_done");

	for (int32 i = 0; i < NumItemSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_c%d"), *P, i);
		B.LoadConst(R3, i).CmpEq(Flag, SlotIndexReg, R3).JumpIf(Flag, BranchLabel);
	}
	B.Jump(DoneLabel);

	for (int32 i = 0; i < NumItemSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_c%d"), *P, i);
		B.Label(BranchLabel).SaveConstEntity(Self, ItemSlotProperties[i], 0).Jump(DoneLabel);
	}

	B.Label(DoneLabel);

	return B;
}

FHktStoryBuilder& HktSnippetItem::ApplyItemStats(
	FHktStoryBuilder& B,
	RegisterIndex ItemEntity,
	RegisterIndex CharEntity)
{
	using namespace Reg;

	B.LoadEntityProperty(R3, ItemEntity, PropertyId::AttackPower)
	 .LoadEntityProperty(R4, CharEntity, PropertyId::AttackPower)
	 .Add(R4, R4, R3)
	 .SaveEntityProperty(CharEntity, PropertyId::AttackPower, R4)
	 .LoadEntityProperty(R3, ItemEntity, PropertyId::Defense)
	 .LoadEntityProperty(R4, CharEntity, PropertyId::Defense)
	 .Add(R4, R4, R3)
	 .SaveEntityProperty(CharEntity, PropertyId::Defense, R4)
	 // Stance: 아이템의 Stance를 캐릭터에 적용
	 .LoadEntityProperty(R3, ItemEntity, PropertyId::Stance)
	 .SaveEntityProperty(CharEntity, PropertyId::Stance, R3);

	return B;
}

FHktStoryBuilder& HktSnippetItem::RemoveItemStats(
	FHktStoryBuilder& B,
	RegisterIndex ItemEntity,
	RegisterIndex CharEntity)
{
	using namespace Reg;

	B.LoadEntityProperty(R0, ItemEntity, PropertyId::AttackPower)
	 .LoadEntityProperty(R1, CharEntity, PropertyId::AttackPower)
	 .Sub(R1, R1, R0)
	 .SaveEntityProperty(CharEntity, PropertyId::AttackPower, R1)
	 .LoadEntityProperty(R0, ItemEntity, PropertyId::Defense)
	 .LoadEntityProperty(R1, CharEntity, PropertyId::Defense)
	 .Sub(R1, R1, R0)
	 .SaveEntityProperty(CharEntity, PropertyId::Defense, R1);

	return B;
}

FHktStoryBuilder& HktSnippetItem::ValidateOwnership(
	FHktStoryBuilder& B,
	RegisterIndex Entity,
	const FString& FailLabel)
{
	using namespace Reg;

	B.LoadEntityProperty(R0, Entity, PropertyId::OwnerEntity)
	 .CmpNe(Flag, R0, Self)
	 .JumpIf(Flag, FailLabel);

	return B;
}

FHktStoryBuilder& HktSnippetItem::ValidateItemState(
	FHktStoryBuilder& B,
	RegisterIndex Entity,
	int32 ExpectedState,
	const FString& FailLabel)
{
	using namespace Reg;

	B.LoadEntityProperty(R0, Entity, PropertyId::ItemState)
	 .LoadConst(R1, ExpectedState)
	 .CmpNe(Flag, R0, R1)
	 .JumpIf(Flag, FailLabel);

	return B;
}
