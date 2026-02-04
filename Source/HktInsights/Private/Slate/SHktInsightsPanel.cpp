// Copyright HKT. All Rights Reserved.

#include "Slate/SHktInsightsPanel.h"
#include "Slate/SHktIntentEventList.h"
#include "Slate/SHktVMStateList.h"
#include "HktInsightsDataCollector.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "HktInsightsPanel"

void SHktInsightsPanel::Construct(const FArguments& InArgs)
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

        // 구분선
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]

        // 필터 패널
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            CreateFilterPanel()
        ]

        // 구분선
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]

        // 메인 컨텐츠 (분할 뷰)
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(4.0f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Vertical)

            // Intent 이벤트 목록
            + SSplitter::Slot()
            .Value(0.5f)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("IntentEventsTitle", "Intent Events"))
                .InitiallyCollapsed(false)
                .BodyContent()
                [
                    SAssignNew(IntentList, SHktIntentEventList)
                ]
            ]

            // VM 상태 목록
            + SSplitter::Slot()
            .Value(0.5f)
            [
                SNew(SExpandableArea)
                .AreaTitle(LOCTEXT("VMStateTitle", "Active VMs"))
                .InitiallyCollapsed(false)
                .BodyContent()
                [
                    SAssignNew(VMList, SHktVMStateList)
                ]
            ]
        ]

        // 구분선
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SSeparator)
        ]

        // 통계 패널
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            CreateStatsPanel()
        ]
    ];

    // 초기 데이터 로드
    RefreshData();
}

void SHktInsightsPanel::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (bAutoRefresh)
    {
        if (InCurrentTime - LastRefreshTime >= AutoRefreshInterval)
        {
            RefreshData();
            LastRefreshTime = InCurrentTime;
        }
    }
}

void SHktInsightsPanel::RefreshData()
{
    FHktInsightsDataCollector& Collector = FHktInsightsDataCollector::Get();

    // 통계 업데이트
    CachedStats = Collector.GetStats();

    // Intent 이벤트 가져오기 및 필터링
    TArray<FHktInsightsIntentEntry> AllIntents = Collector.GetRecentIntentEvents(200);
    CachedIntentEvents.Empty();
    for (const FHktInsightsIntentEntry& Entry : AllIntents)
    {
        if (PassesIntentFilter(Entry) && PassesSearchFilter(Entry))
        {
            CachedIntentEvents.Add(Entry);
        }
    }

    // 활성 VM 가져오기 및 필터링
    TArray<FHktInsightsVMEntry> AllActiveVMs = Collector.GetActiveVMs();
    CachedActiveVMs.Empty();
    for (const FHktInsightsVMEntry& Entry : AllActiveVMs)
    {
        if (bShowActiveVMs && PassesSearchFilter(Entry))
        {
            CachedActiveVMs.Add(Entry);
        }
    }

    // 완료된 VM 가져오기 (옵션)
    CachedCompletedVMs.Empty();
    if (bShowCompletedVMs)
    {
        TArray<FHktInsightsVMEntry> AllCompletedVMs = Collector.GetRecentCompletedVMs(50);
        for (const FHktInsightsVMEntry& Entry : AllCompletedVMs)
        {
            if (PassesSearchFilter(Entry))
            {
                CachedCompletedVMs.Add(Entry);
            }
        }
    }

    // UI 업데이트
    if (IntentList.IsValid())
    {
        IntentList->SetItems(CachedIntentEvents);
    }

    if (VMList.IsValid())
    {
        // Active와 Completed VM을 합쳐서 표시
        TArray<FHktInsightsVMEntry> CombinedVMs = CachedActiveVMs;
        CombinedVMs.Append(CachedCompletedVMs);
        VMList->SetItems(CombinedVMs);
    }
}

void SHktInsightsPanel::ClearData()
{
    FHktInsightsDataCollector::Get().Clear();
    RefreshData();
}

void SHktInsightsPanel::SetAutoRefresh(bool bEnabled)
{
    bAutoRefresh = bEnabled;
}

TSharedRef<SWidget> SHktInsightsPanel::CreateToolbar()
{
    return SNew(SHorizontalBox)

        // 검색 박스
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(2.0f)
        [
            SAssignNew(SearchBox, SSearchBox)
            .HintText(LOCTEXT("SearchHint", "Search events and VMs..."))
            .OnTextChanged(this, &SHktInsightsPanel::OnSearchTextChanged)
            .OnTextCommitted(this, &SHktInsightsPanel::OnSearchTextCommitted)
        ]

        // Pause/Resume 버튼
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text_Lambda([this]() { 
                return bAutoRefresh 
                    ? LOCTEXT("PauseButton", "Pause") 
                    : LOCTEXT("ResumeButton", "Resume"); 
            })
            .OnClicked(this, &SHktInsightsPanel::OnPauseResumeButtonClicked)
        ]

        // Refresh 버튼
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("RefreshButton", "Refresh"))
            .OnClicked_Lambda([this]() { RefreshData(); return FReply::Handled(); })
        ]

        // Clear 버튼
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("ClearButton", "Clear"))
            .OnClicked(this, &SHktInsightsPanel::OnClearButtonClicked)
        ];
}

TSharedRef<SWidget> SHktInsightsPanel::CreateFilterPanel()
{
    return SNew(SHorizontalBox)

        // Intent 필터
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("IntentFilterLabel", "Intent:"))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SHktInsightsPanel::GetFilterCheckState, FString("Pending"))
            .OnCheckStateChanged(this, &SHktInsightsPanel::OnFilterCheckChanged, FString("Pending"))
            [
                SNew(STextBlock).Text(LOCTEXT("PendingFilter", "Pending"))
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SHktInsightsPanel::GetFilterCheckState, FString("Processing"))
            .OnCheckStateChanged(this, &SHktInsightsPanel::OnFilterCheckChanged, FString("Processing"))
            [
                SNew(STextBlock).Text(LOCTEXT("ProcessingFilter", "Processing"))
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SHktInsightsPanel::GetFilterCheckState, FString("Completed"))
            .OnCheckStateChanged(this, &SHktInsightsPanel::OnFilterCheckChanged, FString("Completed"))
            [
                SNew(STextBlock).Text(LOCTEXT("CompletedFilter", "Completed"))
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SHktInsightsPanel::GetFilterCheckState, FString("Failed"))
            .OnCheckStateChanged(this, &SHktInsightsPanel::OnFilterCheckChanged, FString("Failed"))
            [
                SNew(STextBlock).Text(LOCTEXT("FailedFilter", "Failed"))
            ]
        ]

        // 구분자
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 0.0f)
        [
            SNew(SSeparator)
            .Orientation(Orient_Vertical)
        ]

        // VM 필터
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("VMFilterLabel", "VM:"))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SHktInsightsPanel::GetFilterCheckState, FString("ActiveVMs"))
            .OnCheckStateChanged(this, &SHktInsightsPanel::OnFilterCheckChanged, FString("ActiveVMs"))
            [
                SNew(STextBlock).Text(LOCTEXT("ActiveVMsFilter", "Active"))
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SCheckBox)
            .IsChecked(this, &SHktInsightsPanel::GetFilterCheckState, FString("CompletedVMs"))
            .OnCheckStateChanged(this, &SHktInsightsPanel::OnFilterCheckChanged, FString("CompletedVMs"))
            [
                SNew(STextBlock).Text(LOCTEXT("CompletedVMsFilter", "Completed"))
            ]
        ];
}

TSharedRef<SWidget> SHktInsightsPanel::CreateStatsPanel()
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsTotal", "Total Events: {0}"),
                    FText::AsNumber(CachedStats.TotalEventCount));
            })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsPending", "Pending: {0}"),
                    FText::AsNumber(CachedStats.PendingEventCount));
            })
            .ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.0f))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsActiveVMs", "Active VMs: {0}"),
                    FText::AsNumber(CachedStats.ActiveVMCount));
            })
            .ColorAndOpacity(FLinearColor(0.0f, 0.8f, 0.2f))
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsFailed", "Failed: {0}"),
                    FText::AsNumber(CachedStats.FailedEventCount));
            })
            .ColorAndOpacity(FLinearColor(1.0f, 0.2f, 0.2f))
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
            .Text_Lambda([this]() {
                return FText::Format(
                    LOCTEXT("StatsAvgTime", "Avg VM Time: {0}ms"),
                    FText::AsNumber(FMath::RoundToInt(CachedStats.AverageVMExecutionTime * 1000.0f)));
            })
        ];
}

void SHktInsightsPanel::OnSearchTextChanged(const FText& NewText)
{
    CurrentSearchText = NewText.ToString();
    RefreshData();
}

void SHktInsightsPanel::OnSearchTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
    CurrentSearchText = NewText.ToString();
    RefreshData();
}

FReply SHktInsightsPanel::OnClearButtonClicked()
{
    ClearData();
    return FReply::Handled();
}

FReply SHktInsightsPanel::OnPauseResumeButtonClicked()
{
    bAutoRefresh = !bAutoRefresh;
    return FReply::Handled();
}

void SHktInsightsPanel::OnFilterCheckChanged(ECheckBoxState NewState, FString FilterName)
{
    bool bNewValue = (NewState == ECheckBoxState::Checked);

    if (FilterName == "Pending") bShowPendingEvents = bNewValue;
    else if (FilterName == "Processing") bShowProcessingEvents = bNewValue;
    else if (FilterName == "Completed") bShowCompletedEvents = bNewValue;
    else if (FilterName == "Failed") bShowFailedEvents = bNewValue;
    else if (FilterName == "ActiveVMs") bShowActiveVMs = bNewValue;
    else if (FilterName == "CompletedVMs") bShowCompletedVMs = bNewValue;

    RefreshData();
}

ECheckBoxState SHktInsightsPanel::GetFilterCheckState(FString FilterName) const
{
    bool bValue = false;

    if (FilterName == "Pending") bValue = bShowPendingEvents;
    else if (FilterName == "Processing") bValue = bShowProcessingEvents;
    else if (FilterName == "Completed") bValue = bShowCompletedEvents;
    else if (FilterName == "Failed") bValue = bShowFailedEvents;
    else if (FilterName == "ActiveVMs") bValue = bShowActiveVMs;
    else if (FilterName == "CompletedVMs") bValue = bShowCompletedVMs;

    return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SHktInsightsPanel::PassesIntentFilter(const FHktInsightsIntentEntry& Entry) const
{
    switch (Entry.State)
    {
    case EHktInsightsEventState::Pending:
        return bShowPendingEvents;
    case EHktInsightsEventState::Processing:
        return bShowProcessingEvents;
    case EHktInsightsEventState::Completed:
        return bShowCompletedEvents;
    case EHktInsightsEventState::Failed:
    case EHktInsightsEventState::Cancelled:
        return bShowFailedEvents;
    default:
        return true;
    }
}

bool SHktInsightsPanel::PassesVMFilter(const FHktInsightsVMEntry& Entry) const
{
    if (Entry.State == EHktInsightsVMState::Completed || Entry.State == EHktInsightsVMState::Error)
    {
        return bShowCompletedVMs;
    }
    return bShowActiveVMs;
}

bool SHktInsightsPanel::PassesSearchFilter(const FHktInsightsIntentEntry& Entry) const
{
    if (CurrentSearchText.IsEmpty())
    {
        return true;
    }

    // 태그 이름에서 검색
    if (Entry.EventTag.ToString().Contains(CurrentSearchText, ESearchCase::IgnoreCase))
    {
        return true;
    }

    // 이벤트 ID에서 검색
    if (FString::Printf(TEXT("%d"), Entry.EventId).Contains(CurrentSearchText))
    {
        return true;
    }

    // Subject ID에서 검색
    if (FString::Printf(TEXT("%d"), Entry.SubjectId).Contains(CurrentSearchText))
    {
        return true;
    }

    return false;
}

bool SHktInsightsPanel::PassesSearchFilter(const FHktInsightsVMEntry& Entry) const
{
    if (CurrentSearchText.IsEmpty())
    {
        return true;
    }

    // 태그 이름에서 검색
    if (Entry.SourceEventTag.ToString().Contains(CurrentSearchText, ESearchCase::IgnoreCase))
    {
        return true;
    }

    // VM ID에서 검색
    if (FString::Printf(TEXT("%d"), Entry.VMId).Contains(CurrentSearchText))
    {
        return true;
    }

    // Opcode 이름에서 검색
    if (Entry.CurrentOpcodeName.Contains(CurrentSearchText, ESearchCase::IgnoreCase))
    {
        return true;
    }

    return false;
}

#undef LOCTEXT_NAMESPACE
