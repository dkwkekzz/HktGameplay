// Copyright HKT. All Rights Reserved.

#include "Slate/SHktWorldStatePanel.h"
#include "HktRuntimeInsightsCollector.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
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
}

// ============================================================================
// SHktWorldEntityListRow - 엔티티 목록 한 행
// ============================================================================

class SHktWorldEntityListRow : public SMultiColumnTableRow<TSharedPtr<FHktEntityListEntry>>
{
public:
    SLATE_BEGIN_ARGS(SHktWorldEntityListRow) {}
        SLATE_ARGUMENT(TSharedPtr<FHktEntityListEntry>, Item)
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

        return SNullWidget::NullWidget;
    }

private:
    TSharedPtr<FHktEntityListEntry> Item;
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

        // 필터바
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            CreateFilterBar()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]

        // 메인 컨텐츠 (엔티티 목록 + 상세)
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

            // 상세 패널 (35%)
            + SSplitter::Slot()
            .Value(0.35f)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("DetailTitle", "Detail  (select entities)"))
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

    RefreshData(/*bForceRebuild=*/ true);
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

TSharedRef<SWidget> SHktWorldStatePanel::CreateFilterBar()
{
    return SNew(SHorizontalBox)

        // Source 필터
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("SourceLabel", "Source:"))
        ]

        + SHorizontalBox::Slot()
        .MaxWidth(80.0f)
        .Padding(2.0f)
        [
            SNew(SEditableTextBox)
            .HintText(LOCTEXT("SourceHint", "All"))
            .OnTextChanged(this, &SHktWorldStatePanel::OnSourceFilterChanged)
        ]

        // Type 필터
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("TypeLabel", "Type:"))
        ]

        + SHorizontalBox::Slot()
        .MaxWidth(80.0f)
        .Padding(2.0f)
        [
            SNew(SEditableTextBox)
            .HintText(LOCTEXT("TypeHint", "All"))
            .OnTextChanged(this, &SHktWorldStatePanel::OnTypeFilterChanged)
        ]

        // ID 필터
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("IdLabel", "ID:"))
        ]

        + SHorizontalBox::Slot()
        .MaxWidth(60.0f)
        .Padding(2.0f)
        [
            SNew(SEditableTextBox)
            .HintText(LOCTEXT("IdHint", "All"))
            .OnTextChanged(this, &SHktWorldStatePanel::OnEntityIdFilterChanged)
        ]

        // 일반 검색
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(2.0f)
        [
            SNew(SSearchBox)
            .HintText(LOCTEXT("SearchHint", "Search..."))
            .OnTextChanged(this, &SHktWorldStatePanel::OnSearchTextChanged)
        ]

        // Pause/Resume
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

        // Clear
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
    return SAssignNew(EntityListView, SListView<TSharedPtr<FHktEntityListEntry>>)
        .ListItemsSource(&EntityListItems)
        .OnGenerateRow(this, &SHktWorldStatePanel::GenerateEntityRow)
        .OnSelectionChanged(this, &SHktWorldStatePanel::OnEntitySelectionChanged)
        .SelectionMode(ESelectionMode::Multi)
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
            .FillWidth(1.0f)
        );
}

TSharedRef<SWidget> SHktWorldStatePanel::CreateDetailSection()
{
    return SAssignNew(DetailScrollBox, SScrollBox);
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
                TArray<TSharedPtr<FHktEntityListEntry>> SelectedItems = EntityListView.IsValid()
                    ? EntityListView->GetSelectedItems() : TArray<TSharedPtr<FHktEntityListEntry>>();
                if (SelectedItems.Num() == 0)
                {
                    return FText::FromString(TEXT("No entity selected"));
                }
                return FText::FromString(FString::Printf(TEXT("Selected: %d entities"),
                    SelectedItems.Num()));
            })
            .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
        ];
}

// ============================================================================
// 행 생성
// ============================================================================

TSharedRef<ITableRow> SHktWorldStatePanel::GenerateEntityRow(
    TSharedPtr<FHktEntityListEntry> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SHktWorldEntityListRow, OwnerTable).Item(Item);
}

// ============================================================================
// 필터
// ============================================================================

bool SHktWorldStatePanel::PassesFilter(const FHktEntityListEntry& Entry) const
{
    if (!SourceFilter.IsEmpty() &&
        !Entry.Source.Contains(SourceFilter, ESearchCase::IgnoreCase))
    {
        return false;
    }
    if (!TypeFilter.IsEmpty() &&
        !Entry.TypeName.Contains(TypeFilter, ESearchCase::IgnoreCase))
    {
        return false;
    }
    if (!EntityIdFilter.IsEmpty())
    {
        const FString IdStr = FString::FromInt(Entry.EntityId);
        if (!IdStr.Contains(EntityIdFilter))
        {
            return false;
        }
    }
    if (!SearchText.IsEmpty())
    {
        const FString IdStr = FString::FromInt(Entry.EntityId);
        const bool bMatch = Entry.Source.Contains(SearchText, ESearchCase::IgnoreCase)
            || IdStr.Contains(SearchText)
            || Entry.TypeName.Contains(SearchText, ESearchCase::IgnoreCase);
        if (!bMatch) return false;
    }
    return true;
}

// ============================================================================
// 데이터 갱신
// ============================================================================

void SHktWorldStatePanel::RefreshData(bool bForceRebuild)
{
    FHktRuntimeInsightsCollector& Collector = FHktRuntimeInsightsCollector::Get();

    // 엔티티 리스트 변경 감지
    const int32 CurrentVersion = Collector.GetEntityListVersion();
    if (bForceRebuild || CurrentVersion != CachedEntityListVersion)
    {
        CachedEntityListVersion = CurrentVersion;
        RebuildEntityList();
    }

    // 상세 데이터 갱신 (위젯 재생성 없음 — Text_Lambda가 자동 반영)
    UpdateDetailData();

    // 위젯 구조가 바뀐 경우에만 재생성
    if (bDetailWidgetsDirty)
    {
        bDetailWidgetsDirty = false;
        RebuildDetailWidgets();
    }
}

void SHktWorldStatePanel::RebuildEntityList()
{
    FHktRuntimeInsightsCollector& Collector = FHktRuntimeInsightsCollector::Get();
    TArray<FHktEntityListEntry> RawList = Collector.GetEntityList();

    // 소스 카운트
    TSet<FString> Sources;
    for (const FHktEntityListEntry& E : RawList)
    {
        Sources.Add(E.Source);
    }
    CachedNumSources = Sources.Num();
    CachedTotalEntities = RawList.Num();

    // 선택 상태 백업
    TSet<int64> PrevSelectedKeys;
    if (EntityListView.IsValid())
    {
        for (const TSharedPtr<FHktEntityListEntry>& Sel : EntityListView->GetSelectedItems())
        {
            if (Sel.IsValid())
            {
                int64 Key = GetTypeHash(Sel->Source) ^ (static_cast<int64>(Sel->EntityId) << 32);
                PrevSelectedKeys.Add(Key);
            }
        }
    }

    EntityListItems.Reset();

    for (FHktEntityListEntry& Entry : RawList)
    {
        if (!PassesFilter(Entry)) continue;
        EntityListItems.Add(MakeShared<FHktEntityListEntry>(MoveTemp(Entry)));
    }

    // 선택 상태 복원
    TArray<TSharedPtr<FHktEntityListEntry>> RestoredSelection;
    if (PrevSelectedKeys.Num() > 0)
    {
        for (const TSharedPtr<FHktEntityListEntry>& Row : EntityListItems)
        {
            int64 Key = GetTypeHash(Row->Source) ^ (static_cast<int64>(Row->EntityId) << 32);
            if (PrevSelectedKeys.Contains(Key))
            {
                RestoredSelection.Add(Row);
            }
        }
    }

    if (EntityListView.IsValid())
    {
        EntityListView->RequestListRefresh();

        if (RestoredSelection.Num() > 0)
        {
            EntityListView->ClearSelection();
            for (const TSharedPtr<FHktEntityListEntry>& Sel : RestoredSelection)
            {
                EntityListView->SetItemSelection(Sel, true, ESelectInfo::Direct);
            }
        }
    }
}

// ============================================================================
// 상세 패널 — 데이터만 갱신 (위젯 재생성 없음)
// ============================================================================

void SHktWorldStatePanel::UpdateDetailData()
{
    if (!EntityListView.IsValid()) return;

    FHktRuntimeInsightsCollector& Collector = FHktRuntimeInsightsCollector::Get();
    TArray<TSharedPtr<FHktEntityListEntry>> SelectedItems = EntityListView->GetSelectedItems();

    // 선택 수 변경 → 위젯 구조 재생성 필요
    if (SelectedItems.Num() != CachedDetailEntries.Num())
    {
        bDetailWidgetsDirty = true;
    }

    CachedDetailEntries.SetNum(SelectedItems.Num());

    for (int32 i = 0; i < SelectedItems.Num(); ++i)
    {
        const TSharedPtr<FHktEntityListEntry>& SelEntry = SelectedItems[i];
        if (!SelEntry.IsValid()) continue;

        if (!CachedDetailEntries[i].IsValid())
        {
            CachedDetailEntries[i] = MakeShared<FHktSelectedEntityDetail>();
            bDetailWidgetsDirty = true;
        }

        FHktSelectedEntityDetail& Cached = *CachedDetailEntries[i];

        // 엔티티 자체가 바뀌었으면 위젯 재생성
        if (Cached.EntityId != SelEntry->EntityId || Cached.Source != SelEntry->Source)
        {
            bDetailWidgetsDirty = true;
        }

        // Collector에서 최신 데이터 가져오기
        TArray<FHktSelectedEntityDetail> AllDetails = Collector.GetAllEntityDetails(SelEntry->Source);
        for (const FHktSelectedEntityDetail& D : AllDetails)
        {
            if (D.EntityId == SelEntry->EntityId)
            {
                // 프로퍼티 수 변경 → 위젯 구조 재생성
                if (Cached.PropNames.Num() != D.PropNames.Num())
                {
                    bDetailWidgetsDirty = true;
                }
                Cached = D; // 값 복사 (Text_Lambda가 다음 페인트에서 반영)
                break;
            }
        }
    }
}

// ============================================================================
// 상세 패널 — 위젯 구조 재생성 (선택 변경/프로퍼티 수 변경 시에만)
//   모든 값 표시에 Text_Lambda 사용 → CachedDetailEntries 갱신만으로 화면 반영
// ============================================================================

void SHktWorldStatePanel::RebuildDetailWidgets()
{
    if (!DetailScrollBox.IsValid() || !EntityListView.IsValid()) return;

    DetailScrollBox->ClearChildren();

    TArray<TSharedPtr<FHktEntityListEntry>> SelectedItems = EntityListView->GetSelectedItems();

    for (int32 Idx = 0; Idx < CachedDetailEntries.Num(); ++Idx)
    {
        if (Idx >= SelectedItems.Num()) break;
        const TSharedPtr<FHktEntityListEntry>& SelEntry = SelectedItems[Idx];
        if (!SelEntry.IsValid()) continue;

        TSharedPtr<FHktSelectedEntityDetail> Entry = CachedDetailEntries[Idx];
        if (!Entry.IsValid()) continue;

        // 헤더 텍스트
        const FString Header = FString::Printf(TEXT("[%s] Entity #%d (%s)"),
            *SelEntry->Source, SelEntry->EntityId, *SelEntry->TypeName);

        TSharedRef<SVerticalBox> PropList = SNew(SVerticalBox);

        if (Entry->IsValid())
        {
            // OwnerUid
            PropList->AddSlot()
            .AutoHeight()
            .Padding(8.0f, 1.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(160.0f)
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT("OwnerUid")))
                        .ColorAndOpacity(FLinearColor(0.5f, 0.8f, 1.0f))
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SNew(STextBlock).Text_Lambda([Entry]()
                    {
                        return FText::FromString(FString::Printf(TEXT("%lld"), Entry->OwnerUid));
                    })
                ]
            ];

            // Frame
            PropList->AddSlot()
            .AutoHeight()
            .Padding(8.0f, 1.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(160.0f)
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT("Frame")))
                        .ColorAndOpacity(FLinearColor(0.5f, 0.8f, 1.0f))
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SNew(STextBlock).Text_Lambda([Entry]()
                    {
                        return FText::FromString(FString::Printf(TEXT("%lld"), Entry->FrameNumber));
                    })
                ]
            ];

            // Properties (Text_Lambda로 동적 값 표시)
            for (int32 PropIdx = 0; PropIdx < Entry->PropNames.Num(); ++PropIdx)
            {
                PropList->AddSlot()
                .AutoHeight()
                .Padding(8.0f, 1.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(160.0f)
                        [
                            SNew(STextBlock).Text_Lambda([Entry, PropIdx]()
                            {
                                return PropIdx < Entry->PropNames.Num()
                                    ? FText::FromString(Entry->PropNames[PropIdx])
                                    : FText::GetEmpty();
                            })
                            .ColorAndOpacity(FLinearColor(0.5f, 0.8f, 1.0f))
                        ]
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SNew(STextBlock).Text_Lambda([Entry, PropIdx]()
                        {
                            return PropIdx < Entry->PropValues.Num()
                                ? FText::FromString(FString::FromInt(Entry->PropValues[PropIdx]))
                                : FText::GetEmpty();
                        })
                    ]
                ];
            }
        }
        else
        {
            PropList->AddSlot()
            .AutoHeight()
            .Padding(8.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("NoDetail", "No detail data available"))
                .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
            ];
        }

        DetailScrollBox->AddSlot()
        [
            SNew(SExpandableArea)
            .AreaTitle(FText::FromString(Header))
            .AreaTitleFont(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            .InitiallyCollapsed(false)
            .HeaderPadding(FMargin(4.0f, 2.0f))
            .BodyContent()
            [
                PropList
            ]
        ];
    }
}

// ============================================================================
// 콜백
// ============================================================================

void SHktWorldStatePanel::OnEntitySelectionChanged(TSharedPtr<FHktEntityListEntry> Item, ESelectInfo::Type SelectType)
{
    if (SelectType == ESelectInfo::Direct) return;
    bDetailWidgetsDirty = true;
}

void SHktWorldStatePanel::OnSourceFilterChanged(const FText& NewText)
{
    SourceFilter = NewText.ToString();
    RefreshData(/*bForceRebuild=*/ true);
}

void SHktWorldStatePanel::OnTypeFilterChanged(const FText& NewText)
{
    TypeFilter = NewText.ToString();
    RefreshData(/*bForceRebuild=*/ true);
}

void SHktWorldStatePanel::OnEntityIdFilterChanged(const FText& NewText)
{
    EntityIdFilter = NewText.ToString();
    RefreshData(/*bForceRebuild=*/ true);
}

void SHktWorldStatePanel::OnSearchTextChanged(const FText& NewText)
{
    SearchText = NewText.ToString();
    RefreshData(/*bForceRebuild=*/ true);
}

FReply SHktWorldStatePanel::OnPauseResumeClicked()
{
    bAutoRefresh = !bAutoRefresh;
    return FReply::Handled();
}

FReply SHktWorldStatePanel::OnClearClicked()
{
    FHktRuntimeInsightsCollector::Get().Clear();
    CachedEntityListVersion = -1;
    RefreshData(/*bForceRebuild=*/ true);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
