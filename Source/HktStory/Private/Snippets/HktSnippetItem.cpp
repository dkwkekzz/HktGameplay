// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Snippets/HktSnippetItem.h"
#include "HktCoreProperties.h"
#include "HktStoryEventParams.h"

static constexpr uint16 EquipSlotProperties[] =
{
	PropertyId::EquipSlot0, PropertyId::EquipSlot1, PropertyId::EquipSlot2,
	PropertyId::EquipSlot3, PropertyId::EquipSlot4, PropertyId::EquipSlot5,
	PropertyId::EquipSlot6, PropertyId::EquipSlot7, PropertyId::EquipSlot8,
};

static constexpr int32 NumEquipSlots = UE_ARRAY_COUNT(EquipSlotProperties);

FHktStoryBuilder& HktSnippetItem::LoadItemFromSlot(
	FHktStoryBuilder& B,
	RegisterIndex DstReg,
	const FString& FailLabel)
{
	using namespace Reg;

	FString P = B.MakeInternalLabel(TEXT("slot"));
	FString DoneLabel = P + TEXT("_done");

	// 슬롯 인덱스 로드 (UseSkillParams::EquipSlotIndex = Param1)
	B.LoadStore(R0, UseSkillParams::EquipSlotIndex);                        // R0 = 슬롯 인덱스

	// 디스패치: 각 슬롯 인덱스에 대해 비교 + 점프
	for (int32 i = 0; i < NumEquipSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_ld%d"), *P, i);
		B.LoadConst(R1, i).CmpEq(Flag, R0, R1).JumpIf(Flag, BranchLabel);
	}
	B.Jump(FailLabel);                                                      // 유효하지 않은 슬롯

	// 로드 타겟
	for (int32 i = 0; i < NumEquipSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_ld%d"), *P, i);
		B.Label(BranchLabel).LoadStore(DstReg, EquipSlotProperties[i]).Jump(DoneLabel);
	}

	B.Label(DoneLabel);

	// 유효성 검증: 아이템 엔티티 != 0
	B.LoadConst(R3, 0)
	 .CmpEq(Flag, DstReg, R3)
	 .JumpIf(Flag, FailLabel);

	return B;
}

FHktStoryBuilder& HktSnippetItem::SaveItemToEquipSlot(
	FHktStoryBuilder& B,
	RegisterIndex SlotIndexReg,
	RegisterIndex ValueReg)
{
	using namespace Reg;

	FString P = B.MakeInternalLabel(TEXT("sslot"));
	FString DoneLabel = P + TEXT("_done");

	for (int32 i = 0; i < NumEquipSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_s%d"), *P, i);
		B.LoadConst(R4, i).CmpEq(Flag, SlotIndexReg, R4).JumpIf(Flag, BranchLabel);
	}
	B.Jump(DoneLabel);

	for (int32 i = 0; i < NumEquipSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_s%d"), *P, i);
		B.Label(BranchLabel).SaveEntityProperty(Self, EquipSlotProperties[i], ValueReg).Jump(DoneLabel);
	}

	B.Label(DoneLabel);

	return B;
}

FHktStoryBuilder& HktSnippetItem::ClearEquipSlot(
	FHktStoryBuilder& B,
	RegisterIndex SlotIndexReg)
{
	using namespace Reg;

	FString P = B.MakeInternalLabel(TEXT("cslot"));
	FString DoneLabel = P + TEXT("_done");

	for (int32 i = 0; i < NumEquipSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_c%d"), *P, i);
		B.LoadConst(R3, i).CmpEq(Flag, SlotIndexReg, R3).JumpIf(Flag, BranchLabel);
	}
	B.Jump(DoneLabel);

	for (int32 i = 0; i < NumEquipSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_c%d"), *P, i);
		B.Label(BranchLabel).SaveConstEntity(Self, EquipSlotProperties[i], 0).Jump(DoneLabel);
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

FHktStoryBuilder& HktSnippetItem::FindEmptyEquipSlot(
	FHktStoryBuilder& B,
	RegisterIndex DstReg,
	const FString& FailLabel)
{
	using namespace Reg;

	FString P = B.MakeInternalLabel(TEXT("fslot"));
	FString FoundLabel = P + TEXT("_found");

	// EquipSlot0~8 순차 검사: 값이 0이면 빈 슬롯
	for (int32 i = 0; i < NumEquipSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_f%d"), *P, i);
		B.LoadStore(R4, EquipSlotProperties[i])
		 .LoadConst(R5, 0)
		 .CmpEq(Flag, R4, R5)
		 .JumpIf(Flag, BranchLabel);
	}
	B.Jump(FailLabel);  // 모든 슬롯이 차 있음

	for (int32 i = 0; i < NumEquipSlots; ++i)
	{
		FString BranchLabel = FString::Printf(TEXT("%s_f%d"), *P, i);
		B.Label(BranchLabel)
		 .LoadConst(DstReg, i)
		 .Jump(FoundLabel);
	}

	B.Label(FoundLabel);

	return B;
}

// ================================================================
// 고수준 아이템 명령어
// ================================================================

FHktStoryBuilder& HktSnippetItem::AssignOwnership(
	FHktStoryBuilder& B,
	RegisterIndex ItemEntity,
	RegisterIndex NewOwner)
{
	B.Log(TEXT("[Snippet] AssignOwnership"))
	 .SaveEntityProperty(ItemEntity, PropertyId::OwnerEntity, NewOwner)
	 .SetOwnerUid(ItemEntity);

	return B;
}

FHktStoryBuilder& HktSnippetItem::ReleaseOwnership(
	FHktStoryBuilder& B,
	RegisterIndex ItemEntity)
{
	B.Log(TEXT("[Snippet] ReleaseOwnership"))
	 .SaveConstEntity(ItemEntity, PropertyId::OwnerEntity, 0)
	 .ClearOwnerUid(ItemEntity);

	return B;
}

FHktStoryBuilder& HktSnippetItem::ActivateInSlot(
	FHktStoryBuilder& B,
	RegisterIndex ItemEntity,
	RegisterIndex SlotIndexReg,
	RegisterIndex CharEntity)
{
	using namespace Reg;

	// Active 상태로 전환 + EquipIndex 설정
	B.Log(TEXT("[Snippet] ActivateInSlot"))
	 .SaveConstEntity(ItemEntity, PropertyId::ItemState, 2)              // Active
	 .SaveEntityProperty(ItemEntity, PropertyId::EquipIndex, SlotIndexReg);

	// 캐릭터의 EquipSlot[N] = 아이템 EntityId
	// Note: SlotIndexReg과 R3를 분리하여 레지스터 충돌 방지
	B.Move(R3, ItemEntity);
	SaveItemToEquipSlot(B, SlotIndexReg, R3);

	// 아이템 스탯 + Stance를 캐릭터에 적용
	ApplyItemStats(B, ItemEntity, CharEntity);

	return B;
}

FHktStoryBuilder& HktSnippetItem::DeactivateToBag(
	FHktStoryBuilder& B,
	RegisterIndex ItemEntity,
	RegisterIndex CharEntity)
{
	// InBag 상태로 전환 + EquipIndex 해제
	B.Log(TEXT("[Snippet] DeactivateToBag"))
	 .SaveConstEntity(ItemEntity, PropertyId::ItemState, 1)              // InBag
	 .SaveConstEntity(ItemEntity, PropertyId::EquipIndex, -1);           // 액션 해제

	// 아이템 스탯을 캐릭터에서 차감
	RemoveItemStats(B, ItemEntity, CharEntity);

	return B;
}

FHktStoryBuilder& HktSnippetItem::DropToGround(
	FHktStoryBuilder& B,
	RegisterIndex ItemEntity,
	RegisterIndex PositionSourceEntity)
{
	using namespace Reg;

	// Ground로 전환
	B.Log(TEXT("[Snippet] DropToGround"))
	 .SaveConstEntity(ItemEntity, PropertyId::ItemState, 0)              // Ground
	 .SaveConstEntity(ItemEntity, PropertyId::EquipIndex, -1);           // 장착 해제

	// 소유권 해제
	ReleaseOwnership(B, ItemEntity);

	// 위치 설정
	B.GetPosition(R3, PositionSourceEntity)
	 .SetPosition(ItemEntity, R3);

	return B;
}

FHktStoryBuilder& HktSnippetItem::SpawnGroundItem(
	FHktStoryBuilder& B,
	const FGameplayTag& ItemClassTag,
	const FHktGroundItemTemplate& Template,
	RegisterIndex PosSourceEntity)
{
	using namespace Reg;

	B.Log(TEXT("[Snippet] SpawnGroundItem"))
	 .SpawnEntity(ItemClassTag)
	 .SaveConstEntity(Spawned, PropertyId::ItemState, 0)                 // Ground
	 .SaveConstEntity(Spawned, PropertyId::ItemId, Template.ItemId)
	 .SaveConstEntity(Spawned, PropertyId::EquipIndex, -1)               // 미등록
	 .GetPosition(R3, PosSourceEntity)
	 .SetPosition(Spawned, R3);

	return B;
}
