// Copyright HKT. All Rights Reserved.

#include "Slate/SHktRuntimeInsightsPanel.h"
#include "HktRuntimeInsightsCollector.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "HktRuntimeInsightsPanel"

// 컬럼 이름
namespace ProviderColumns
{
    static const FName Category("Category");
    static const FName Key("Key");
    static const FName Value("Value");
}

namespace PacketColumns
{
    static const FName Time("Time");
    static const FName Direction("Dir");
    static const FName Type("Type");
    static const FName Player("Player");
    static const FName Frame("Frame");
    static const FName Events("Events");
    static const FName Size("Size");
    static const FName Desc("Desc");
}

// ============================================================================
// SHktRuntimeInsightsPanel
// ============================================================================

void SHktRuntimeInsightsPanel::Construct(const FArguments& InArgs)
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
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(2.0f)
            [
                SAssignNew(SearchBox, SSearchBox)
                .HintText(LOCTEXT("SearchHint", "Filter by category, key, or value..."))
                .OnTextChanged(this, &SHktRuntimeInsightsPanel::OnSearchTextChanged)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            [
                SNew(SButton)
                .Text_Lambda([this]() {
                    return bAutoRefresh
                        ? LOCTEXT("Pause", "Pause")
                        : LOCTEXT("Resume", "Resume");
                })
                .OnClicked(this, &SHktRuntimeInsightsPanel::OnPauseResumeClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Clear", "Clear"))
                .OnClicked(this, &SHktRuntimeInsightsPanel::OnClearClicked)
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]

        // 메인 컨텐츠 (수직 분할)
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(4.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Vertical)

            // Provider 스냅샷 섹션
            + SSplitter::Slot()
            .Value(0.55f)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("ProviderTitle", "Runtime State"))
                .InitiallyCollapsed(false)
                .BodyContent()
                [
                    CreateProviderSection()
                ]
            ]

            // Packet 트래픽 섹션
            + SSplitter::Slot()
            .Value(0.45f)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("PacketTitle", "Packet Traffic"))
                .InitiallyCollapsed(false)
                .BodyContent()
                [
                    CreatePacketSection()
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
        .Padding(4.0f)
        [
            CreateStatsBar()
        ]
    ];

    RefreshData();
}

void SHktRuntimeInsightsPanel::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (bAutoRefresh && (InCurrentTime - LastRefreshTime >= AutoRefreshInterval))
    {
        RefreshData();
        LastRefreshTime = InCurrentTime;
    }
}

// ============================================================================
// Provider 섹션
// ============================================================================

TSharedRef<SWidget> SHktRuntimeInsightsPanel::CreateProviderSection()
{
    return SAssignNew(ProviderListView, SListView<TSharedPtr<FHktInsightEntry>>)
        .ListItemsSource(&ProviderListItems)
        .OnGenerateRow(this, &SHktRuntimeInsightsPanel::GenerateProviderRow)
        .SelectionMode(ESelectionMode::Single)
        .HeaderRow
        (
            SNew(SHeaderRow)

            + SHeaderRow::Column(ProviderColumns::Category)
            .DefaultLabel(LOCTEXT("CategoryCol", "Category"))
            .FixedWidth(140.0f)

            + SHeaderRow::Column(ProviderColumns::Key)
            .DefaultLabel(LOCTEXT("KeyCol", "Key"))
            .FillWidth(0.4f)

            + SHeaderRow::Column(ProviderColumns::Value)
            .DefaultLabel(LOCTEXT("ValueCol", "Value"))
            .FillWidth(0.6f)
        );
}

TSharedRef<ITableRow> SHktRuntimeInsightsPanel::GenerateProviderRow(
    TSharedPtr<FHktInsightEntry> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SHktProviderEntryRow, OwnerTable)
        .Item(Item);
}

// ============================================================================
// Packet 섹션
// ============================================================================

TSharedRef<SWidget> SHktRuntimeInsightsPanel::CreatePacketSection()
{
    return SAssignNew(PacketListView, SListView<TSharedPtr<FHktPacketRecord>>)
        .ListItemsSource(&PacketListItems)
        .OnGenerateRow(this, &SHktRuntimeInsightsPanel::GeneratePacketRow)
        .SelectionMode(ESelectionMode::Single)
        .HeaderRow
        (
            SNew(SHeaderRow)

            + SHeaderRow::Column(PacketColumns::Time)
            .DefaultLabel(LOCTEXT("TimeCol", "Time"))
            .FixedWidth(70.0f)

            + SHeaderRow::Column(PacketColumns::Direction)
            .DefaultLabel(LOCTEXT("DirCol", "Dir"))
            .FixedWidth(40.0f)

            + SHeaderRow::Column(PacketColumns::Type)
            .DefaultLabel(LOCTEXT("TypeCol", "Type"))
            .FixedWidth(90.0f)

            + SHeaderRow::Column(PacketColumns::Player)
            .DefaultLabel(LOCTEXT("PlayerCol", "Player"))
            .FixedWidth(80.0f)

            + SHeaderRow::Column(PacketColumns::Frame)
            .DefaultLabel(LOCTEXT("FrameCol", "Frame"))
            .FixedWidth(60.0f)

            + SHeaderRow::Column(PacketColumns::Events)
            .DefaultLabel(LOCTEXT("EventsCol", "Evts"))
            .FixedWidth(45.0f)

            + SHeaderRow::Column(PacketColumns::Size)
            .DefaultLabel(LOCTEXT("SizeCol", "Size"))
            .FixedWidth(60.0f)

            + SHeaderRow::Column(PacketColumns::Desc)
            .DefaultLabel(LOCTEXT("DescCol", "Description"))
            .FillWidth(1.0f)
        );
}

TSharedRef<ITableRow> SHktRuntimeInsightsPanel::GeneratePacketRow(
    TSharedPtr<FHktPacketRecord> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SHktPacketRecordRow, OwnerTable)
        .Item(Item);
}

// ============================================================================
// 통계 바
// ============================================================================

TSharedRef<SWidget> SHktRuntimeInsightsPanel::CreateStatsBar()
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsProviders", "Providers: {0}"),
                    FText::AsNumber(CachedProviderCount));
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsPkts", "Total Pkts: {0}"),
                    FText::AsNumber(CachedStats.TotalPackets));
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsPPS", "Pkts/s: {0}"),
                    FText::AsNumber(FMath::RoundToInt(CachedStats.PacketsPerSecond)));
            })
            .ColorAndOpacity(FLinearColor(0.0f, 0.8f, 0.2f))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                float KBps = CachedStats.BytesPerSecond / 1024.0f;
                return FText::FromString(FString::Printf(TEXT("%.1f KB/s"), KBps));
            })
            .ColorAndOpacity(FLinearColor(0.0f, 0.6f, 1.0f))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsC2S", "C→S: {0}"),
                    FText::AsNumber(CachedStats.C2S_Count));
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsS2C", "S→C: {0}"),
                    FText::AsNumber(CachedStats.S2C_Count));
            })
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNullWidget::NullWidget
        ];
}

// ============================================================================
// 데이터 갱신
// ============================================================================

void SHktRuntimeInsightsPanel::RefreshData()
{
    FHktRuntimeInsightsCollector& Collector = FHktRuntimeInsightsCollector::Get();

    // Provider 데이터 수집
    Collector.CollectAll();
    const TArray<FHktInsightSnapshot>& Snapshots = Collector.GetSnapshots();

    ProviderListItems.Reset();
    CachedProviderCount = Snapshots.Num();

    for (const FHktInsightSnapshot& Snapshot : Snapshots)
    {
        for (const FHktInsightEntry& Entry : Snapshot.Entries)
        {
            // 검색 필터
            if (!SearchText.IsEmpty())
            {
                if (!Entry.Category.Contains(SearchText, ESearchCase::IgnoreCase) &&
                    !Entry.Key.Contains(SearchText, ESearchCase::IgnoreCase) &&
                    !Entry.Value.Contains(SearchText, ESearchCase::IgnoreCase))
                {
                    continue;
                }
            }

            ProviderListItems.Add(MakeShared<FHktInsightEntry>(Entry));
        }
    }

    if (ProviderListView.IsValid())
    {
        ProviderListView->RequestListRefresh();
    }

    // Packet 데이터
    TArray<FHktPacketRecord> Packets = Collector.GetRecentPackets(200);
    PacketListItems.Reset();
    for (const FHktPacketRecord& Pkt : Packets)
    {
        if (!SearchText.IsEmpty())
        {
            if (!Pkt.GetTypeString().Contains(SearchText, ESearchCase::IgnoreCase) &&
                !Pkt.Description.Contains(SearchText, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }
        PacketListItems.Add(MakeShared<FHktPacketRecord>(Pkt));
    }

    if (PacketListView.IsValid())
    {
        PacketListView->RequestListRefresh();
    }

    // 통계
    CachedStats = Collector.GetPacketStats();
}

// ============================================================================
// 콜백
// ============================================================================

void SHktRuntimeInsightsPanel::OnSearchTextChanged(const FText& NewText)
{
    SearchText = NewText.ToString();
    RefreshData();
}

FReply SHktRuntimeInsightsPanel::OnPauseResumeClicked()
{
    bAutoRefresh = !bAutoRefresh;
    return FReply::Handled();
}

FReply SHktRuntimeInsightsPanel::OnClearClicked()
{
    FHktRuntimeInsightsCollector::Get().Clear();
    RefreshData();
    return FReply::Handled();
}

// ============================================================================
// SHktProviderEntryRow
// ============================================================================

void SHktProviderEntryRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
{
    Item = InArgs._Item;
    SMultiColumnTableRow<TSharedPtr<FHktInsightEntry>>::Construct(
        FSuperRowType::FArguments(), InOwnerTable);
}

TSharedRef<SWidget> SHktProviderEntryRow::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid()) return SNullWidget::NullWidget;

    // Severity에 따른 색상
    FLinearColor TextColor = FLinearColor::White;
    if (Item->Severity == 1) TextColor = FLinearColor(1.0f, 0.8f, 0.0f); // Warning
    else if (Item->Severity >= 2) TextColor = FLinearColor(1.0f, 0.2f, 0.2f); // Error

    if (ColumnName == ProviderColumns::Category)
    {
        return SNew(SBox).Padding(FMargin(4.0f, 2.0f)).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(FText::FromString(Item->Category))
            .ColorAndOpacity(FLinearColor(0.5f, 0.8f, 1.0f))
        ];
    }

    if (ColumnName == ProviderColumns::Key)
    {
        return SNew(SBox).Padding(FMargin(4.0f, 2.0f)).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(FText::FromString(Item->Key))
            .ColorAndOpacity(TextColor)
        ];
    }

    if (ColumnName == ProviderColumns::Value)
    {
        return SNew(SBox).Padding(FMargin(4.0f, 2.0f)).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(FText::FromString(Item->Value))
            .ColorAndOpacity(TextColor)
            .ToolTipText(FText::FromString(Item->Value))
        ];
    }

    return SNullWidget::NullWidget;
}

// ============================================================================
// SHktPacketRecordRow
// ============================================================================

void SHktPacketRecordRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
{
    Item = InArgs._Item;
    SMultiColumnTableRow<TSharedPtr<FHktPacketRecord>>::Construct(
        FSuperRowType::FArguments(), InOwnerTable);
}

TSharedRef<SWidget> SHktPacketRecordRow::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid()) return SNullWidget::NullWidget;

    FLinearColor DirColor = (Item->Direction == EHktPacketDirection::ClientToServer)
        ? FLinearColor(0.0f, 0.8f, 0.8f)  // Cyan for C2S
        : FLinearColor(1.0f, 0.6f, 0.0f); // Orange for S2C

    auto MakeText = [](const FString& Str, FLinearColor Color = FLinearColor::White) -> TSharedRef<SWidget>
    {
        return SNew(SBox).Padding(FMargin(4.0f, 2.0f)).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(FText::FromString(Str)).ColorAndOpacity(Color)
        ];
    };

    if (ColumnName == PacketColumns::Time)
    {
        return MakeText(FString::Printf(TEXT("%.3f"), Item->Timestamp));
    }
    if (ColumnName == PacketColumns::Direction)
    {
        return MakeText(Item->GetDirectionString(), DirColor);
    }
    if (ColumnName == PacketColumns::Type)
    {
        return MakeText(Item->GetTypeString(), DirColor);
    }
    if (ColumnName == PacketColumns::Player)
    {
        return MakeText(Item->PlayerUid != 0 ? FString::Printf(TEXT("%lld"), Item->PlayerUid) : TEXT("-"));
    }
    if (ColumnName == PacketColumns::Frame)
    {
        return MakeText(Item->FrameNumber > 0 ? FString::Printf(TEXT("%lld"), Item->FrameNumber) : TEXT("-"));
    }
    if (ColumnName == PacketColumns::Events)
    {
        return MakeText(FString::FromInt(Item->EventCount));
    }
    if (ColumnName == PacketColumns::Size)
    {
        return MakeText(FString::Printf(TEXT("%dB"), Item->EstimatedSizeBytes));
    }
    if (ColumnName == PacketColumns::Desc)
    {
        return MakeText(Item->Description);
    }

    return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
