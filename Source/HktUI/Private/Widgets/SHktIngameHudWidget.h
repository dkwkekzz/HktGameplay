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

class APlayerController;

/**
 * 인게임 뷰포트 HUD 위젯.
 * 하단 바에 Inventory/Equipment/Skills 버튼과 각각의 패널을 제공합니다.
 */
class HKTUI_API SHktIngameHudWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktIngameHudWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetOwningPlayerController(APlayerController* InPC)
	{
		CachedPC = InPC;
	}

private:
	FReply OnInventoryClicked();
	FReply OnEquipmentClicked();
	FReply OnSkillsClicked();
	void TogglePanel(int32 PanelIndex);

	TSharedPtr<SBorder> InventoryPanel;
	TSharedPtr<SBorder> EquipmentPanel;
	TSharedPtr<SBorder> SkillsPanel;

	TWeakObjectPtr<APlayerController> CachedPC;

	int32 ActivePanel = -1; // -1 = none
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline void SHktIngameHudWidget::Construct(const FArguments& InArgs)
{
	// --- 인벤토리 패널 ---
	auto BuildInventoryPanel = [this]() -> TSharedRef<SWidget>
	{
		TSharedRef<SScrollBox> ItemList = SNew(SScrollBox);

		// Mock 아이템 데이터
		static const TArray<FString> Items = { TEXT("Iron Sword"), TEXT("Wooden Shield"), TEXT("Health Potion x3"), TEXT("Mana Potion x2"), TEXT("Leather Armor"), TEXT("Magic Ring") };
		for (const FString& Item : Items)
		{
			ItemList->AddSlot()
			[
				SNew(SBox)
				.Padding(FMargin(8.f, 4.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item))
					.ColorAndOpacity(FLinearColor::White)
				]
			];
		}

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
						ItemList
					]
				]
			];
	};

	// --- 장착 패널 ---
	auto BuildEquipmentPanel = [this]() -> TSharedRef<SWidget>
	{
		struct FEquipSlot { FString Name; FString Item; };
		static const TArray<FEquipSlot> Slots = {
			{ TEXT("Head"), TEXT("Leather Helmet") },
			{ TEXT("Body"), TEXT("Leather Armor") },
			{ TEXT("Weapon"), TEXT("Iron Sword") },
			{ TEXT("Shield"), TEXT("Wooden Shield") },
			{ TEXT("Accessory"), TEXT("Empty") }
		};

		TSharedRef<SVerticalBox> SlotList = SNew(SVerticalBox);
		for (const FEquipSlot& Slot : Slots)
		{
			SlotList->AddSlot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SBox).WidthOverride(80.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Slot.Name))
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Slot.Item))
					.ColorAndOpacity(Slot.Item == TEXT("Empty") ? FLinearColor(0.4f, 0.4f, 0.4f) : FLinearColor::White)
				]
			];
		}

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
						SlotList
					]
				]
			];
	};

	// --- 스킬 패널 ---
	auto BuildSkillsPanel = [this]() -> TSharedRef<SWidget>
	{
		struct FSkillSlot { FString Name; FString Desc; FString Cooldown; };
		static const TArray<FSkillSlot> Skills = {
			{ TEXT("Fireball"), TEXT("Deal fire damage to target"), TEXT("8s") },
			{ TEXT("Heal"), TEXT("Restore health"), TEXT("12s") },
			{ TEXT("Dash"), TEXT("Quick movement"), TEXT("5s") },
			{ TEXT("Shield Bash"), TEXT("Stun target briefly"), TEXT("10s") }
		};

		TSharedRef<SVerticalBox> SkillList = SNew(SVerticalBox);
		for (const FSkillSlot& Skill : Skills)
		{
			SkillList->AddSlot()
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
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Skill.Name))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FLinearColor(1.f, 0.8f, 0.3f))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("CD: %s"), *Skill.Cooldown)))
							.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Skill.Desc))
						.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
					]
				]
			];
		}

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
						.Text(FText::FromString(TEXT("Skills")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
						.ColorAndOpacity(FLinearColor(1.f, 0.4f, 0.4f))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SkillList
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

inline FReply SHktIngameHudWidget::OnInventoryClicked() { TogglePanel(0); return FReply::Handled(); }
inline FReply SHktIngameHudWidget::OnEquipmentClicked() { TogglePanel(1); return FReply::Handled(); }
inline FReply SHktIngameHudWidget::OnSkillsClicked() { TogglePanel(2); return FReply::Handled(); }

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
