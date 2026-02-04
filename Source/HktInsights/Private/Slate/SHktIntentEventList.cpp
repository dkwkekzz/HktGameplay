// Copyright HKT. All Rights Reserved.

#include "Slate/SHktIntentEventList.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "HktIntentEventList"

// 컬럼 이름 정의
namespace IntentColumnNames
{
    static const FName ID("ID");
    static const FName Frame("Frame");
    static const FName Tag("Tag");
    static const FName Subject("Subject");
    static const FName Target("Target");
    static const FName State("State");
    static const FName Time("Time");
}

void SHktIntentEventList::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SVerticalBox)

        // 리스트 뷰
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SAssignNew(ListView, SListView<TSharedPtr<FHktInsightsIntentEntry>>)
            .ListItemsSource(&ListItems)
            .OnGenerateRow(this, &SHktIntentEventList::GenerateRow)
            .OnSelectionChanged(this, &SHktIntentEventList::OnSelectionChanged)
            .SelectionMode(ESelectionMode::Single)
            .HeaderRow
            (
                SNew(SHeaderRow)

                + SHeaderRow::Column(IntentColumnNames::ID)
                .DefaultLabel(LOCTEXT("IDColumn", "ID"))
                .FixedWidth(60.0f)

                + SHeaderRow::Column(IntentColumnNames::Frame)
                .DefaultLabel(LOCTEXT("FrameColumn", "Frame"))
                .FixedWidth(60.0f)

                + SHeaderRow::Column(IntentColumnNames::Tag)
                .DefaultLabel(LOCTEXT("TagColumn", "Event Tag"))
                .FillWidth(1.0f)

                + SHeaderRow::Column(IntentColumnNames::Subject)
                .DefaultLabel(LOCTEXT("SubjectColumn", "Subject"))
                .FixedWidth(70.0f)

                + SHeaderRow::Column(IntentColumnNames::Target)
                .DefaultLabel(LOCTEXT("TargetColumn", "Target"))
                .FixedWidth(70.0f)

                + SHeaderRow::Column(IntentColumnNames::State)
                .DefaultLabel(LOCTEXT("StateColumn", "State"))
                .FixedWidth(90.0f)

                + SHeaderRow::Column(IntentColumnNames::Time)
                .DefaultLabel(LOCTEXT("TimeColumn", "Time"))
                .FixedWidth(70.0f)
            )
        ]
    ];
}

void SHktIntentEventList::SetItems(const TArray<FHktInsightsIntentEntry>& Items)
{
    ListItems.Empty(Items.Num());

    for (const FHktInsightsIntentEntry& Item : Items)
    {
        ListItems.Add(MakeShared<FHktInsightsIntentEntry>(Item));
    }

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

TSharedPtr<FHktInsightsIntentEntry> SHktIntentEventList::GetSelectedItem() const
{
    TArray<TSharedPtr<FHktInsightsIntentEntry>> SelectedItems = ListView->GetSelectedItems();
    return SelectedItems.Num() > 0 ? SelectedItems[0] : nullptr;
}

TSharedRef<ITableRow> SHktIntentEventList::GenerateRow(
    TSharedPtr<FHktInsightsIntentEntry> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SHktIntentEventRow, OwnerTable)
        .Item(Item);
}

void SHktIntentEventList::OnSelectionChanged(
    TSharedPtr<FHktInsightsIntentEntry> SelectedItem,
    ESelectInfo::Type SelectInfo)
{
    OnIntentSelected.ExecuteIfBound(SelectedItem);
}

TSharedRef<SWidget> SHktIntentEventList::CreateHeaderRow()
{
    return SNew(SHeaderRow)
        + SHeaderRow::Column(IntentColumnNames::ID)
        .DefaultLabel(LOCTEXT("IDColumn", "ID"))
        .FixedWidth(60.0f)

        + SHeaderRow::Column(IntentColumnNames::Frame)
        .DefaultLabel(LOCTEXT("FrameColumn", "Frame"))
        .FixedWidth(60.0f)

        + SHeaderRow::Column(IntentColumnNames::Tag)
        .DefaultLabel(LOCTEXT("TagColumn", "Event Tag"))
        .FillWidth(1.0f)

        + SHeaderRow::Column(IntentColumnNames::Subject)
        .DefaultLabel(LOCTEXT("SubjectColumn", "Subject"))
        .FixedWidth(70.0f)

        + SHeaderRow::Column(IntentColumnNames::Target)
        .DefaultLabel(LOCTEXT("TargetColumn", "Target"))
        .FixedWidth(70.0f)

        + SHeaderRow::Column(IntentColumnNames::State)
        .DefaultLabel(LOCTEXT("StateColumn", "State"))
        .FixedWidth(90.0f)

        + SHeaderRow::Column(IntentColumnNames::Time)
        .DefaultLabel(LOCTEXT("TimeColumn", "Time"))
        .FixedWidth(70.0f);
}

// ========== SHktIntentEventRow ==========

void SHktIntentEventRow::Construct(
    const FArguments& InArgs,
    const TSharedRef<STableViewBase>& InOwnerTable)
{
    Item = InArgs._Item;

    SMultiColumnTableRow<TSharedPtr<FHktInsightsIntentEntry>>::Construct(
        FSuperRowType::FArguments(),
        InOwnerTable);
}

TSharedRef<SWidget> SHktIntentEventRow::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    if (ColumnName == IntentColumnNames::ID)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::AsNumber(Item->EventId))
            ];
    }

    if (ColumnName == IntentColumnNames::Frame)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::AsNumber(Item->FrameNumber))
            ];
    }

    if (ColumnName == IntentColumnNames::Tag)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->EventTag.ToString()))
                .ToolTipText(FText::FromString(Item->EventTag.ToString()))
            ];
    }

    if (ColumnName == IntentColumnNames::Subject)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::AsNumber(Item->SubjectId))
            ];
    }

    if (ColumnName == IntentColumnNames::Target)
    {
        FString TargetStr = Item->TargetId != INDEX_NONE 
            ? FString::Printf(TEXT("%d"), Item->TargetId)
            : TEXT("-");

        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TargetStr))
            ];
    }

    if (ColumnName == IntentColumnNames::State)
    {
        // 상태에 따른 색상 지정
        FLinearColor StateColor = Item->GetStateColor();
        FString StateStr = Item->GetStateString();

        // 상태 아이콘 추가
        FString IconStr;
        switch (Item->State)
        {
        case EHktInsightsEventState::Pending:    IconStr = TEXT("● "); break;
        case EHktInsightsEventState::Processing: IconStr = TEXT("▶ "); break;
        case EHktInsightsEventState::Completed:  IconStr = TEXT("✓ "); break;
        case EHktInsightsEventState::Failed:     IconStr = TEXT("✗ "); break;
        case EHktInsightsEventState::Cancelled:  IconStr = TEXT("○ "); break;
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

    if (ColumnName == IntentColumnNames::Time)
    {
        return SNew(SBox)
            .Padding(FMargin(4.0f, 2.0f))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%.2fs"), Item->Timestamp)))
            ];
    }

    return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
