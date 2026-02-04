// Copyright HKT. All Rights Reserved.

#include "Slate/SHktVMStateList.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"

#define LOCTEXT_NAMESPACE "HktVMStateList"

// 컬럼 이름 정의
namespace VMColumnNames
{
    static const FName VMId("VMId");
    static const FName EventId("EventId");
    static const FName Tag("Tag");
    static const FName Progress("Progress");
    static const FName Opcode("Opcode");
    static const FName State("State");
    static const FName Time("Time");
}

void SHktVMStateList::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SVerticalBox)

        // 리스트 뷰
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SAssignNew(ListView, SListView<TSharedPtr<FHktInsightsVMEntry>>)
            .ListItemsSource(&ListItems)
            .OnGenerateRow(this, &SHktVMStateList::GenerateRow)
            .OnSelectionChanged(this, &SHktVMStateList::OnSelectionChanged)
            .SelectionMode(ESelectionMode::Single)
            .HeaderRow
            (
                SNew(SHeaderRow)

                + SHeaderRow::Column(VMColumnNames::VMId)
                .DefaultLabel(LOCTEXT("VMIdColumn", "VM"))
                .FixedWidth(50.0f)

                + SHeaderRow::Column(VMColumnNames::EventId)
                .DefaultLabel(LOCTEXT("EventIdColumn", "Event"))
                .FixedWidth(60.0f)

                + SHeaderRow::Column(VMColumnNames::Tag)
                .DefaultLabel(LOCTEXT("TagColumn", "Event Tag"))
                .FillWidth(0.8f)

                + SHeaderRow::Column(VMColumnNames::Progress)
                .DefaultLabel(LOCTEXT("ProgressColumn", "Progress"))
                .FillWidth(0.6f)

                + SHeaderRow::Column(VMColumnNames::Opcode)
                .DefaultLabel(LOCTEXT("OpcodeColumn", "Current Op"))
                .FillWidth(0.5f)

                + SHeaderRow::Column(VMColumnNames::State)
                .DefaultLabel(LOCTEXT("StateColumn", "State"))
                .FixedWidth(80.0f)

                + SHeaderRow::Column(VMColumnNames::Time)
                .DefaultLabel(LOCTEXT("TimeColumn", "Time"))
                .FixedWidth(70.0f)
            )
        ]
    ];
}

void SHktVMStateList::SetItems(const TArray<FHktInsightsVMEntry>& Items)
{
    ListItems.Empty(Items.Num());

    for (const FHktInsightsVMEntry& Item : Items)
    {
        ListItems.Add(MakeShared<FHktInsightsVMEntry>(Item));
    }

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

TSharedPtr<FHktInsightsVMEntry> SHktVMStateList::GetSelectedItem() const
{
    TArray<TSharedPtr<FHktInsightsVMEntry>> SelectedItems = ListView->GetSelectedItems();
    return SelectedItems.Num() > 0 ? SelectedItems[0] : nullptr;
}

TSharedRef<ITableRow> SHktVMStateList::GenerateRow(
    TSharedPtr<FHktInsightsVMEntry> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SHktVMStateRow, OwnerTable)
        .Item(Item);
}

void SHktVMStateList::OnSelectionChanged(
    TSharedPtr<FHktInsightsVMEntry> SelectedItem,
    ESelectInfo::Type SelectInfo)
{
    OnVMSelected.ExecuteIfBound(SelectedItem);
}

// ========== SHktVMStateRow ==========

void SHktVMStateRow::Construct(
    const FArguments& InArgs,
    const TSharedRef<STableViewBase>& InOwnerTable)
{
    Item = InArgs._Item;

    SMultiColumnTableRow<TSharedPtr<FHktInsightsVMEntry>>::Construct(
        FSuperRowType::FArguments(),
        InOwnerTable);
}

TSharedRef<SWidget> SHktVMStateRow::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    if (ColumnName == VMColumnNames::VMId)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("VM:%d"), Item->VMId)))
            ];
    }

    if (ColumnName == VMColumnNames::EventId)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::AsNumber(Item->SourceEventId))
            ];
    }

    if (ColumnName == VMColumnNames::Tag)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->SourceEventTag.ToString()))
                .ToolTipText(FText::FromString(Item->SourceEventTag.ToString()))
            ];
    }

    if (ColumnName == VMColumnNames::Progress)
    {
        return CreateProgressBar();
    }

    if (ColumnName == VMColumnNames::Opcode)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->CurrentOpcodeName.IsEmpty() ? TEXT("-") : Item->CurrentOpcodeName))
            ];
    }

    if (ColumnName == VMColumnNames::State)
    {
        // 상태에 따른 색상 지정
        FLinearColor StateColor = Item->GetStateColor();
        FString StateStr = Item->GetStateString();

        // 상태 아이콘 추가
        FString IconStr;
        switch (Item->State)
        {
        case EHktInsightsVMState::Running:   IconStr = TEXT("▶ "); break;
        case EHktInsightsVMState::Blocked:   IconStr = TEXT("⏸ "); break;
        case EHktInsightsVMState::Moving:    IconStr = TEXT("→ "); break;
        case EHktInsightsVMState::Completed: IconStr = TEXT("✓ "); break;
        case EHktInsightsVMState::Error:     IconStr = TEXT("✗ "); break;
        default: break;
        }

        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(IconStr + StateStr))
                .ColorAndOpacity(StateColor)
            ];
    }

    if (ColumnName == VMColumnNames::Time)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.2fs"), Item->ElapsedTime)))
            ];
    }

    return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SHktVMStateRow::CreateProgressBar()
{
    float Progress = Item->GetProgress();
    int32 ProgressPercent = FMath::RoundToInt(Progress * 100.0f);

    // 상태에 따른 색상
    FLinearColor BarColor;
    switch (Item->State)
    {
    case EHktInsightsVMState::Running:
        BarColor = FLinearColor(0.0f, 0.8f, 0.2f);  // Green
        break;
    case EHktInsightsVMState::Blocked:
        BarColor = FLinearColor(1.0f, 0.5f, 0.0f);  // Orange
        break;
    case EHktInsightsVMState::Moving:
        BarColor = FLinearColor(0.0f, 0.6f, 1.0f);  // Blue
        break;
    case EHktInsightsVMState::Completed:
        BarColor = FLinearColor(0.5f, 0.5f, 0.5f);  // Gray
        break;
    case EHktInsightsVMState::Error:
        BarColor = FLinearColor(1.0f, 0.2f, 0.2f);  // Red
        break;
    default:
        BarColor = FLinearColor::White;
        break;
    }

    return SNew(SBox)
        .Padding(FMargin(4.0f, 2.0f))
        .VAlign(VAlign_Center)
        [
            SNew(SHorizontalBox)

            // 진행률 바
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SProgressBar)
                .Percent(Progress)
                .FillColorAndOpacity(BarColor)
            ]

            // PC/Size 텍스트
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%d/%d"), Item->ProgramCounter, Item->BytecodeSize)))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
