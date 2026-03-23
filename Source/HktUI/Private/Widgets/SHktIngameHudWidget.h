// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Styling/CoreStyle.h"
#include "IHktPlayerInteractionInterface.h"
#include "HktCoreProperties.h"
#include "GameplayTagsManager.h"

class APlayerController;

/**
 * 인게임 뷰포트 HUD 위젯.
 * 하단 바에 Inventory/Equipment/Skills 버튼과 각각의 패널을 제공합니다.
 * Skills 패널은 실제 액션 슬롯 바인딩을 WorldState에서 읽어 표시합니다.
 */
class HKTUI_API SHktIngameHudWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktIngameHudWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetOwningPlayerController(APlayerController* InPC);

private:
	FReply OnInventoryClicked();
	FReply OnEquipmentClicked();
	FReply OnSkillsClicked();
	void TogglePanel(int32 PanelIndex);

	/** 슬롯 바인딩 변경 시 스킬 패널 갱신 */
	void RefreshSkillsPanel();
	void RefreshInventoryPanel();
	void RefreshEquipmentPanel();

	/** WorldState에서 엔티티의 EntitySpawnTag를 읽어 마지막 세그먼트 이름 반환 */
	static FString GetEntityDisplayName(const FHktWorldState* WS, FHktEntityId Id);

	TSharedPtr<SBorder> InventoryPanel;
	TSharedPtr<SBorder> EquipmentPanel;
	TSharedPtr<SBorder> SkillsPanel;
	TSharedPtr<SVerticalBox> InventoryListBox;
	TSharedPtr<SVerticalBox> EquipmentListBox;
	TSharedPtr<SVerticalBox> SkillListBox;

	TWeakObjectPtr<APlayerController> CachedPC;
	FDelegateHandle SlotBindingHandle;

	int32 ActivePanel = -1; // -1 = none
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline void SHktIngameHudWidget::Construct(const FArguments& InArgs)
{
	// --- 인벤토리 패널 (WorldState 기반) ---
	auto BuildInventoryPanel = [this]() -> TSharedRef<SWidget>
	{
		InventoryListBox = SNew(SVerticalBox);

		return SAssignNew(InventoryPanel, SBorder)
			.Padding(12.f)
			.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.15f, 0.9f))
			.Visibility(EVisibility::Collapsed)
			[
				SNew(SBox)
				.WidthOverride(250.f)
				.HeightOverride(300.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 8.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Inventory")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
						.ColorAndOpacity(FLinearColor::Yellow)
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						InventoryListBox.ToSharedRef()
					]
				]
			];
	};

	// --- 장착 패널 (WorldState 기반) ---
	auto BuildEquipmentPanel = [this]() -> TSharedRef<SWidget>
	{
		EquipmentListBox = SNew(SVerticalBox);

		return SAssignNew(EquipmentPanel, SBorder)
			.Padding(12.f)
			.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.15f, 0.9f))
			.Visibility(EVisibility::Collapsed)
			[
				SNew(SBox)
				.WidthOverride(250.f)
				.HeightOverride(300.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 8.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Equipment")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
						.ColorAndOpacity(FLinearColor(0.3f, 0.8f, 1.f))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						EquipmentListBox.ToSharedRef()
					]
				]
			];
	};

	// --- 스킬 패널 (데이터 기반) ---
	auto BuildSkillsPanel = [this]() -> TSharedRef<SWidget>
	{
		SkillListBox = SNew(SVerticalBox);

		return SAssignNew(SkillsPanel, SBorder)
			.Padding(12.f)
			.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.15f, 0.9f))
			.Visibility(EVisibility::Collapsed)
			[
				SNew(SBox)
				.WidthOverride(280.f)
				.HeightOverride(350.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 8.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Action Slots")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
						.ColorAndOpacity(FLinearColor(1.f, 0.4f, 0.4f))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SkillListBox.ToSharedRef()
					]
				]
			];
	};

	// --- 메인 레이아웃 ---
	ChildSlot
	[
		SNew(SOverlay)

		// 패널들 (하단 바 위)
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(10.f, 0.f, 0.f, 50.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f) [ BuildInventoryPanel() ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f) [ BuildEquipmentPanel() ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f) [ BuildSkillsPanel() ]
		]

		// 하단 버튼 바
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0.f, 0.f, 0.f, 5.f)
		[
			SNew(SBorder)
			.Padding(FMargin(8.f, 4.f))
			.BorderBackgroundColor(FLinearColor(0.08f, 0.08f, 0.12f, 0.85f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f)
				[
					SNew(SBox).WidthOverride(100.f).HeightOverride(32.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center).VAlign(VAlign_Center)
						.OnClicked(this, &SHktIngameHudWidget::OnInventoryClicked)
						[ SNew(STextBlock).Text(FText::FromString(TEXT("Inventory"))) ]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f)
				[
					SNew(SBox).WidthOverride(100.f).HeightOverride(32.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center).VAlign(VAlign_Center)
						.OnClicked(this, &SHktIngameHudWidget::OnEquipmentClicked)
						[ SNew(STextBlock).Text(FText::FromString(TEXT("Equipment"))) ]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.f)
				[
					SNew(SBox).WidthOverride(100.f).HeightOverride(32.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center).VAlign(VAlign_Center)
						.OnClicked(this, &SHktIngameHudWidget::OnSkillsClicked)
						[ SNew(STextBlock).Text(FText::FromString(TEXT("Skills"))) ]
					]
				]
			]
		]
	];
}

inline void SHktIngameHudWidget::SetOwningPlayerController(APlayerController* InPC)
{
	CachedPC = InPC;

	// 슬롯 바인딩 변경 구독 (아이템 상태 변화 시 모든 패널 갱신)
	if (InPC)
	{
		if (IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(InPC))
		{
			SlotBindingHandle = Interaction->OnSlotBindingChanged().AddLambda([this](int32 /*SlotIndex*/)
			{
				RefreshInventoryPanel();
				RefreshEquipmentPanel();
				RefreshSkillsPanel();
			});
		}
	}
}

// ============================================================================
// Helper: EntitySpawnTag → display name
// ============================================================================
inline FString SHktIngameHudWidget::GetEntityDisplayName(const FHktWorldState* WS, FHktEntityId Id)
{
	int32 SpawnTagIdx = WS->GetProperty(Id, PropertyId::EntitySpawnTag);
	if (SpawnTagIdx > 0)
	{
		FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(SpawnTagIdx));
		if (!TagName.IsNone())
		{
			FString Name = TagName.ToString();
			int32 DotIdx;
			if (Name.FindLastChar(TEXT('.'), DotIdx))
			{
				Name = Name.Mid(DotIdx + 1);
			}
			return Name;
		}
	}
	return FString::Printf(TEXT("Item#%d"), WS->GetProperty(Id, PropertyId::ItemId));
}

// ============================================================================
// Inventory 패널 (가방 아이템: ItemState == 1)
// ============================================================================
inline void SHktIngameHudWidget::RefreshInventoryPanel()
{
	if (!InventoryListBox.IsValid()) return;
	InventoryListBox->ClearChildren();

	APlayerController* PC = CachedPC.Get();
	if (!PC) return;

	IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(PC);
	if (!Interaction) return;

	const FHktWorldState* WS = nullptr;
	if (!Interaction->GetWorldState(WS) || !WS) return;

	const int64 PlayerUid = Interaction->GetPlayerUid();
	if (PlayerUid == 0) return;

	// 내 캐릭터 엔티티 찾기
	FHktEntityId MyCharacter = InvalidEntityId;
	WS->ForEachEntityByOwner(PlayerUid, [&](FHktEntityId Id, int32 /*Slot*/)
	{
		if (MyCharacter != InvalidEntityId) return;
		if (WS->GetProperty(Id, PropertyId::ItemState) == 0)
		{
			MyCharacter = Id;
		}
	});
	if (MyCharacter == InvalidEntityId) return;

	// 가방 아이템 수집 (ItemState == 1)
	struct FBagItem { FHktEntityId EntityId; int32 BagSlot; FString Name; int32 AttackPower; };
	TArray<FBagItem> Items;

	// 사용 중인 ActionSlot 수집 (Activate 시 빈 슬롯 자동 할당용)
	TSet<int32> UsedSlots;

	WS->ForEachEntityByOwner(PlayerUid, [&](FHktEntityId ItemId, int32 /*Slot*/)
	{
		if (WS->GetProperty(ItemId, PropertyId::OwnerEntity) != MyCharacter) return;
		const int32 State = WS->GetProperty(ItemId, PropertyId::ItemState);

		if (State == 2) // Active — ActionSlot 사용 중
		{
			int32 Slot = WS->GetProperty(ItemId, PropertyId::ActionSlot);
			if (Slot >= 0) UsedSlots.Add(Slot);
		}

		if (State != 1) return; // InBag만

		Items.Add({
			ItemId,
			WS->GetProperty(ItemId, PropertyId::BagSlot),
			GetEntityDisplayName(WS, ItemId),
			WS->GetProperty(ItemId, PropertyId::AttackPower)
		});
	});

	Items.Sort([](const FBagItem& A, const FBagItem& B) { return A.BagSlot < B.BagSlot; });

	if (Items.Num() == 0)
	{
		InventoryListBox->AddSlot()
		.AutoHeight().Padding(0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Bag is empty")))
			.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
		];
		return;
	}

	// 빈 ActionSlot 찾기 함수
	auto FindNextFreeSlot = [&UsedSlots]() -> int32
	{
		for (int32 S = 0; S < 10; ++S)
		{
			if (!UsedSlots.Contains(S)) return S;
		}
		return 0;
	};

	for (const FBagItem& Item : Items)
	{
		const FHktEntityId ItemEntityId = Item.EntityId;
		const int32 NextSlot = FindNextFreeSlot();

		InventoryListBox->AddSlot()
		.AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.OnClicked_Lambda([this, ItemEntityId, NextSlot]() -> FReply
			{
				if (APlayerController* PC = CachedPC.Get())
				{
					if (IHktPlayerInteractionInterface* I = Cast<IHktPlayerInteractionInterface>(PC))
					{
						I->RequestItemActivate(ItemEntityId, NextSlot);
					}
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SBox).WidthOverride(30.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("[%d]"), Item.BagSlot)))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item.Name))
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item.AttackPower > 0 ? FString::Printf(TEXT("ATK %d"), Item.AttackPower) : TEXT("")))
					.ColorAndOpacity(FLinearColor(1.f, 0.6f, 0.3f))
				]
			]
		];
	}
}

// ============================================================================
// Equipment 패널 (장착 아이템: ItemState == 2)
// ============================================================================
inline void SHktIngameHudWidget::RefreshEquipmentPanel()
{
	if (!EquipmentListBox.IsValid()) return;
	EquipmentListBox->ClearChildren();

	APlayerController* PC = CachedPC.Get();
	if (!PC) return;

	IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(PC);
	if (!Interaction) return;

	const FHktWorldState* WS = nullptr;
	if (!Interaction->GetWorldState(WS) || !WS) return;

	const int64 PlayerUid = Interaction->GetPlayerUid();
	if (PlayerUid == 0) return;

	// 내 캐릭터 엔티티 찾기
	FHktEntityId MyCharacter = InvalidEntityId;
	WS->ForEachEntityByOwner(PlayerUid, [&](FHktEntityId Id, int32 /*Slot*/)
	{
		if (MyCharacter != InvalidEntityId) return;
		if (WS->GetProperty(Id, PropertyId::ItemState) == 0)
		{
			MyCharacter = Id;
		}
	});
	if (MyCharacter == InvalidEntityId) return;

	// 장착 아이템 수집 (ItemState == 2)
	struct FEquipItem { FHktEntityId EntityId; int32 ActionSlot; FString Name; int32 AttackPower; };
	TArray<FEquipItem> Items;

	WS->ForEachEntityByOwner(PlayerUid, [&](FHktEntityId ItemId, int32 /*Slot*/)
	{
		if (WS->GetProperty(ItemId, PropertyId::OwnerEntity) != MyCharacter) return;
		if (WS->GetProperty(ItemId, PropertyId::ItemState) != 2) return; // Active만

		Items.Add({
			ItemId,
			WS->GetProperty(ItemId, PropertyId::ActionSlot),
			GetEntityDisplayName(WS, ItemId),
			WS->GetProperty(ItemId, PropertyId::AttackPower)
		});
	});

	Items.Sort([](const FEquipItem& A, const FEquipItem& B) { return A.ActionSlot < B.ActionSlot; });

	if (Items.Num() == 0)
	{
		EquipmentListBox->AddSlot()
		.AutoHeight().Padding(0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Nothing equipped")))
			.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
		];
		return;
	}

	for (const FEquipItem& Item : Items)
	{
		FString SlotLabel = Item.ActionSlot >= 0
			? FString::Printf(TEXT("Slot %d"), Item.ActionSlot)
			: TEXT("Passive");

		const FHktEntityId ItemEntityId = Item.EntityId;

		EquipmentListBox->AddSlot()
		.AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.OnClicked_Lambda([this, ItemEntityId]() -> FReply
			{
				if (APlayerController* PC = CachedPC.Get())
				{
					if (IHktPlayerInteractionInterface* I = Cast<IHktPlayerInteractionInterface>(PC))
					{
						I->RequestItemDeactivate(ItemEntityId);
					}
				}
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SBox).WidthOverride(60.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(SlotLabel))
						.ColorAndOpacity(FLinearColor(0.5f, 0.8f, 1.f))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item.Name))
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item.AttackPower > 0 ? FString::Printf(TEXT("ATK %d"), Item.AttackPower) : TEXT("")))
					.ColorAndOpacity(FLinearColor(1.f, 0.6f, 0.3f))
				]
			]
		];
	}
}

// ============================================================================
// Skills 패널 (액션 슬롯 바인딩)
// ============================================================================
inline void SHktIngameHudWidget::RefreshSkillsPanel()
{
	if (!SkillListBox.IsValid()) return;
	SkillListBox->ClearChildren();

	APlayerController* PC = CachedPC.Get();
	if (!PC) return;

	IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(PC);
	if (!Interaction) return;

	const FHktWorldState* WS = nullptr;
	if (!Interaction->GetWorldState(WS) || !WS) return;

	const int64 PlayerUid = Interaction->GetPlayerUid();
	if (PlayerUid == 0) return;

	// 내 캐릭터 엔티티 찾기
	FHktEntityId MyCharacter = InvalidEntityId;
	WS->ForEachEntityByOwner(PlayerUid, [&](FHktEntityId Id, int32 /*Slot*/)
	{
		if (MyCharacter != InvalidEntityId) return;
		// 아이템이 아닌 캐릭터 (OwnerEntity가 자기 자신이 아닌 엔티티)
		if (WS->GetProperty(Id, PropertyId::ItemState) == 0)
		{
			MyCharacter = Id;
		}
	});

	if (MyCharacter == InvalidEntityId) return;

	// Active 아이템을 ActionSlot 기준으로 수집
	struct FHktSlotInfo
	{
		int32 ActionSlot;
		FGameplayTag SkillTag;
		int32 CPCost;
		int32 RecoveryFrame;
	};
	TArray<FHktSlotInfo> SlotInfos;

	WS->ForEachEntityByOwner(PlayerUid, [&](FHktEntityId ItemId, int32 /*Slot*/)
	{
		if (WS->GetProperty(ItemId, PropertyId::OwnerEntity) != MyCharacter) return;
		if (WS->GetProperty(ItemId, PropertyId::ItemState) != 2) return; // Active만

		int32 ActionSlot = WS->GetProperty(ItemId, PropertyId::ActionSlot);
		if (ActionSlot < 0) return;

		int32 SkillNetIdx = WS->GetProperty(ItemId, PropertyId::ItemSkillTag);
		FGameplayTag SkillTag;
		if (SkillNetIdx > 0)
		{
			FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(SkillNetIdx));
			if (!TagName.IsNone())
			{
				SkillTag = FGameplayTag::RequestGameplayTag(TagName, false);
			}
		}

		SlotInfos.Add({
			ActionSlot,
			SkillTag,
			WS->GetProperty(ItemId, PropertyId::SkillCPCost),
			WS->GetProperty(ItemId, PropertyId::RecoveryFrame)
		});
	});

	// ActionSlot 순으로 정렬
	SlotInfos.Sort([](const FHktSlotInfo& A, const FHktSlotInfo& B) { return A.ActionSlot < B.ActionSlot; });

	if (SlotInfos.Num() == 0)
	{
		SkillListBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 4.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No active item skills")))
			.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
		];
		return;
	}

	for (const FHktSlotInfo& Info : SlotInfos)
	{
		FString SkillName = Info.SkillTag.IsValid() ? Info.SkillTag.ToString() : TEXT("Unknown");
		// 태그 이름에서 마지막 세그먼트만 표시 (예: "Story.Event.Skill.WoodenSwordSlash" → "WoodenSwordSlash")
		int32 DotIdx;
		if (SkillName.FindLastChar('.', DotIdx))
		{
			SkillName = SkillName.Mid(DotIdx + 1);
		}

		SkillListBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 4.f)
		[
			SNew(SBorder)
			.Padding(8.f)
			.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.2f, 0.8f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("[%d]"), Info.ActionSlot)))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FLinearColor(0.5f, 0.8f, 1.f))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(SkillName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FLinearColor(1.f, 0.8f, 0.3f))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("CP: %d"), Info.CPCost)))
						.ColorAndOpacity(FLinearColor(0.3f, 0.7f, 1.f))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("CD: %d frames"), Info.RecoveryFrame)))
						.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
					]
				]
			]
		];
	}
}

inline FReply SHktIngameHudWidget::OnInventoryClicked()
{
	TogglePanel(0);
	if (ActivePanel == 0) { RefreshInventoryPanel(); }
	return FReply::Handled();
}
inline FReply SHktIngameHudWidget::OnEquipmentClicked()
{
	TogglePanel(1);
	if (ActivePanel == 1) { RefreshEquipmentPanel(); }
	return FReply::Handled();
}
inline FReply SHktIngameHudWidget::OnSkillsClicked()
{
	TogglePanel(2);
	// 스킬 패널 열 때 최신 데이터로 갱신
	if (ActivePanel == 2)
	{
		RefreshSkillsPanel();
	}
	return FReply::Handled();
}

inline void SHktIngameHudWidget::TogglePanel(int32 PanelIndex)
{
	TSharedPtr<SBorder> Panels[] = { InventoryPanel, EquipmentPanel, SkillsPanel };

	if (ActivePanel == PanelIndex)
	{
		// 같은 버튼 → 닫기
		if (Panels[PanelIndex].IsValid())
		{
			Panels[PanelIndex]->SetVisibility(EVisibility::Collapsed);
		}
		ActivePanel = -1;
	}
	else
	{
		// 기존 패널 닫기
		for (int32 i = 0; i < 3; ++i)
		{
			if (Panels[i].IsValid())
			{
				Panels[i]->SetVisibility(EVisibility::Collapsed);
			}
		}
		// 새 패널 열기
		if (Panels[PanelIndex].IsValid())
		{
			Panels[PanelIndex]->SetVisibility(EVisibility::Visible);
		}
		ActivePanel = PanelIndex;
	}
}
