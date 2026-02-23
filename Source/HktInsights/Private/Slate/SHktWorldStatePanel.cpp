// Copyright HKT. All Rights Reserved.

#include "Slate/SHktWorldStatePanel.h"
#include "HktRuntimeInsightsCollector.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "HktWorldStatePanel"

// ============================================================================
// 컬럼 이름 상수
// ============================================================================

namespace WorldStateEntityColumns
{
    static const FName Source    ("Source");
    static const FName EntityId  ("EntityId");
    static const FName TypeName  ("Type");
    static const FName Properties("Properties");
}

namespace WorldStateDetailColumns
{
    static const FName PropName("PropName");
    static const FName Value   ("Value");
}

// ============================================================================
// SHktWorldEntityListRow - 엔티티 목록 한 행
// ============================================================================

class SHktWorldEntityListRow : public SMultiColumnTableRow<TSharedPtr<FHktEntityDisplayRow>>
{
public:
    SLATE_BEGIN_ARGS(SHktWorldEntityListRow) {}
        SLATE_ARGUMENT(TSharedPtr<FHktEntityDisplayRow>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
    {
        Item = InArgs._Item;
        SMultiColumnTableRow::Construct(FSuperRowType::FArguments(), InOwnerTable);
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
    {
        if (!Item.IsValid()) return SNullWidget::NullWidget;

        auto MakeCell = [](const FString& Str, FLinearColor Color) -> TSharedRef<SWidget>
        {
            return SNew(SBox)
                .Padding(FMargin(4.0f, 2.0f))
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Str))
                    .ColorAndOpacity(Color)
                ];
        };

        if (ColumnName == WorldStateEntityColumns::Source)
        {
            // Server → 오렌지, Client → 시안
            const FLinearColor Color = Item->Source.StartsWith(TEXT("Server"))
                ? FLinearColor(1.0f, 0.6f, 0.0f)
                : FLinearColor(0.0f, 0.8f, 0.8f);
            return MakeCell(Item->Source, Color);
        }
        if (ColumnName == WorldStateEntityColumns::EntityId)
        {
            return MakeCell(FString::FromInt(Item->EntityId), FLinearColor::White);
        }
        if (ColumnName == WorldStateEntityColumns::TypeName)
        {
            return MakeCell(Item->TypeName, FLinearColor(0.5f, 0.8f, 1.0f));
        }
        if (ColumnName == WorldStateEntityColumns::Properties)
        {
            return MakeCell(Item->PropSummary, FLinearColor(0.75f, 0.75f, 0.75f));
        }

        return SNullWidget::NullWidget;
    }

private:
    TSharedPtr<FHktEntityDisplayRow> Item;
};

// ============================================================================
// SHktWorldDetailRow - 프로퍼티 상세 한 행
// ============================================================================

class SHktWorldDetailRow : public SMultiColumnTableRow<TSharedPtr<FHktPropPair>>
{
public:
    SLATE_BEGIN_ARGS(SHktWorldDetailRow) {}
        SLATE_ARGUMENT(TSharedPtr<FHktPropPair>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
    {
        Item = InArgs._Item;
        SMultiColumnTableRow::Construct(FSuperRowType::FArguments(), InOwnerTable);
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
    {
        if (!Item.IsValid()) return SNullWidget::NullWidget;

        auto MakeCell = [](const FString& Str, FLinearColor Color) -> TSharedRef<SWidget>
        {
            return SNew(SBox)
                .Padding(FMargin(4.0f, 2.0f))
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Str))
                    .ColorAndOpacity(Color)
                ];
        };

        if (ColumnName == WorldStateDetailColumns::PropName)
        {
            return MakeCell(Item->Key, FLinearColor(0.5f, 0.8f, 1.0f));
        }
        if (ColumnName == WorldStateDetailColumns::Value)
        {
            return MakeCell(Item->Value, FLinearColor::White);
        }

        return SNullWidget::NullWidget;
    }

private:
    TSharedPtr<FHktPropPair> Item;
};

// ============================================================================
// SHktWorldStatePanel - Construct
// ============================================================================

void SHktWorldStatePanel::Construct(const FArguments& InArgs)
{
    AutoRefreshInterval = InArgs._AutoRefreshInterval;

    ChildSlot
    [
        SNew(SVerticalBox)

        // 툴바
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            CreateToolbar()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]

        // 메인 컨텐츠 (엔티티 목록 + 프로퍼티 상세)
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(4.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Vertical)

            // 엔티티 목록 (65%)
            + SSplitter::Slot()
            .Value(0.65f)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("EntityListTitle", "Entities"))
                .InitiallyCollapsed(false)
                .BodyContent()
                [
                    CreateEntitySection()
                ]
            ]

            // 프로퍼티 상세 (35%)
            + SSplitter::Slot()
            .Value(0.35f)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("DetailTitle", "Properties  (엔티티를 클릭하여 상세 확인)"))
                .InitiallyCollapsed(false)
                .BodyContent()
                [
                    CreateDetailSection()
                ]
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]

        // 통계 바
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f, 2.0f)
        [
            CreateStatsBar()
        ]
    ];

    RefreshData();
}

void SHktWorldStatePanel::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (bAutoRefresh && (InCurrentTime - LastRefreshTime >= AutoRefreshInterval))
    {
        RefreshData();
        LastRefreshTime = InCurrentTime;
    }
}

// ============================================================================
// UI 생성
// ============================================================================

TSharedRef<SWidget> SHktWorldStatePanel::CreateToolbar()
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(2.0f)
        [
            SAssignNew(SearchBox, SSearchBox)
            .HintText(LOCTEXT("SearchHint", "소스, EntityId, 타입, 프로퍼티 필터링..."))
            .OnTextChanged(this, &SHktWorldStatePanel::OnSearchTextChanged)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text_Lambda([this]()
            {
                return bAutoRefresh
                    ? LOCTEXT("Pause",  "Pause")
                    : LOCTEXT("Resume", "Resume");
            })
            .OnClicked(this, &SHktWorldStatePanel::OnPauseResumeClicked)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("Clear", "Clear"))
            .OnClicked(this, &SHktWorldStatePanel::OnClearClicked)
        ];
}

TSharedRef<SWidget> SHktWorldStatePanel::CreateEntitySection()
{
    return SAssignNew(EntityListView, SListView<TSharedPtr<FHktEntityDisplayRow>>)
        .ListItemsSource(&EntityListItems)
        .OnGenerateRow(this, &SHktWorldStatePanel::GenerateEntityRow)
        .OnSelectionChanged(this, &SHktWorldStatePanel::OnEntitySelected)
        .SelectionMode(ESelectionMode::Single)
        .HeaderRow
        (
            SNew(SHeaderRow)

            + SHeaderRow::Column(WorldStateEntityColumns::Source)
            .DefaultLabel(LOCTEXT("SourceCol",   "Source"))
            .FixedWidth(90.0f)

            + SHeaderRow::Column(WorldStateEntityColumns::EntityId)
            .DefaultLabel(LOCTEXT("EntityIdCol", "Entity ID"))
            .FixedWidth(75.0f)

            + SHeaderRow::Column(WorldStateEntityColumns::TypeName)
            .DefaultLabel(LOCTEXT("TypeCol",     "Type"))
            .FixedWidth(100.0f)

            + SHeaderRow::Column(WorldStateEntityColumns::Properties)
            .DefaultLabel(LOCTEXT("PropsCol",    "Properties"))
            .FillWidth(1.0f)
        );
}

TSharedRef<SWidget> SHktWorldStatePanel::CreateDetailSection()
{
    return SAssignNew(DetailListView, SListView<TSharedPtr<FHktPropPair>>)
        .ListItemsSource(&DetailListItems)
        .OnGenerateRow(this, &SHktWorldStatePanel::GenerateDetailRow)
        .SelectionMode(ESelectionMode::None)
        .HeaderRow
        (
            SNew(SHeaderRow)

            + SHeaderRow::Column(WorldStateDetailColumns::PropName)
            .DefaultLabel(LOCTEXT("PropNameCol", "Property"))
            .FixedWidth(160.0f)

            + SHeaderRow::Column(WorldStateDetailColumns::Value)
            .DefaultLabel(LOCTEXT("ValueCol",    "Value"))
            .FillWidth(1.0f)
        );
}

TSharedRef<SWidget> SHktWorldStatePanel::CreateStatsBar()
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]()
            {
                return FText::Format(
                    LOCTEXT("StatsSources", "Sources: {0}"),
                    FText::AsNumber(CachedNumSources));
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]()
            {
                return FText::Format(
                    LOCTEXT("StatsEntities", "Entities: {0}"),
                    FText::AsNumber(CachedTotalEntities));
            })
            .ColorAndOpacity(FLinearColor(0.0f, 0.8f, 0.2f))
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNullWidget::NullWidget
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]()
            {
                if (CachedSnapshots.IsEmpty())
                {
                    return FText::FromString(TEXT("No WorldState data"));
                }
                FString Info;
                for (const FHktWorldStateSnapshot& S : CachedSnapshots)
                {
                    if (!Info.IsEmpty()) Info += TEXT("  |  ");
                    Info += FString::Printf(TEXT("%s: F%lld (%d ents)"),
                        *S.SourceName, S.FrameNumber, S.EntityCount);
                }
                return FText::FromString(Info);
            })
            .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
        ];
}

// ============================================================================
// 행 생성
// ============================================================================

TSharedRef<ITableRow> SHktWorldStatePanel::GenerateEntityRow(
    TSharedPtr<FHktEntityDisplayRow> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SHktWorldEntityListRow, OwnerTable).Item(Item);
}

TSharedRef<ITableRow> SHktWorldStatePanel::GenerateDetailRow(
    TSharedPtr<FHktPropPair> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SHktWorldDetailRow, OwnerTable).Item(Item);
}

// ============================================================================
// 데이터 갱신
// ============================================================================

void SHktWorldStatePanel::RefreshData()
{
    FHktRuntimeInsightsCollector& Collector = FHktRuntimeInsightsCollector::Get();

    // CollectAll()을 호출해 Provider들이 WorldState를 push하도록 트리거
    Collector.CollectAll();

    CachedSnapshots = Collector.GetWorldStateSnapshots();
    CachedNumSources = CachedSnapshots.Num();
    CachedTotalEntities = 0;

    EntityListItems.Reset();

    for (const FHktWorldStateSnapshot& Snapshot : CachedSnapshots)
    {
        CachedTotalEntities += Snapshot.EntityCount;

        for (const FHktWorldEntityRow& EntityRow : Snapshot.Entities)
        {
            // 검색 필터 적용
            if (!SearchText.IsEmpty())
            {
                const FString IdStr = FString::FromInt(EntityRow.EntityId);
                const bool bMatchSource = Snapshot.SourceName.Contains(SearchText, ESearchCase::IgnoreCase);
                const bool bMatchId     = IdStr.Contains(SearchText);
                const bool bMatchType   = EntityRow.TypeName.Contains(SearchText, ESearchCase::IgnoreCase);
                const bool bMatchProps  = EntityRow.GetPropSummary(999).Contains(SearchText, ESearchCase::IgnoreCase);

                if (!bMatchSource && !bMatchId && !bMatchType && !bMatchProps)
                {
                    continue;
                }
            }

            TSharedPtr<FHktEntityDisplayRow> DisplayRow = MakeShared<FHktEntityDisplayRow>();
            DisplayRow->Source     = Snapshot.SourceName;
            DisplayRow->EntityId   = EntityRow.EntityId;
            DisplayRow->TypeName   = EntityRow.TypeName;
            DisplayRow->PropSummary = EntityRow.GetPropSummary(5);

            // 상세 패널용 Properties 채우기
            const int32 NumProps = EntityRow.PropNames.Num();
            DisplayRow->Properties.Reserve(NumProps);
            for (int32 i = 0; i < NumProps; ++i)
            {
                DisplayRow->Properties.Emplace(
                    EntityRow.PropNames[i],
                    FString::FromInt(EntityRow.PropValues[i]));
            }

            EntityListItems.Add(MoveTemp(DisplayRow));
        }
    }

    if (EntityListView.IsValid())
    {
        EntityListView->RequestListRefresh();
    }
}

void SHktWorldStatePanel::UpdateDetailPanel(const FHktEntityDisplayRow& Row)
{
    DetailListItems.Reset();

    for (const FHktPropPair& Pair : Row.Properties)
    {
        DetailListItems.Add(MakeShared<FHktPropPair>(Pair.Key, Pair.Value));
    }

    if (DetailListView.IsValid())
    {
        DetailListView->RequestListRefresh();
    }
}

// ============================================================================
// 콜백
// ============================================================================

void SHktWorldStatePanel::OnEntitySelected(TSharedPtr<FHktEntityDisplayRow> Item, ESelectInfo::Type /*SelectType*/)
{
    if (Item.IsValid())
    {
        UpdateDetailPanel(*Item);
    }
}

void SHktWorldStatePanel::OnSearchTextChanged(const FText& NewText)
{
    SearchText = NewText.ToString();
    RefreshData();
}

FReply SHktWorldStatePanel::OnPauseResumeClicked()
{
    bAutoRefresh = !bAutoRefresh;
    return FReply::Handled();
}

FReply SHktWorldStatePanel::OnClearClicked()
{
    FHktRuntimeInsightsCollector::Get().Clear();
    RefreshData();
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
