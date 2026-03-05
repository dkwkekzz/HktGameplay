// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "HktInsightsTypes.h"

class SHktIntentEventList;
class SHktVMStateList;
class SSearchBox;
class SCheckBox;

/**
 * SHktVMStatePanel - Intent 이벤트 + VM 상태 디버그 패널
 *
 * (SHktInsightsPanel에서 이름 변경)
 */
class HKTINSIGHTS_API SHktVMStatePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktVMStatePanel)
        : _AutoRefreshInterval(0.1f)
    {}
        SLATE_ARGUMENT(float, AutoRefreshInterval)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

    void RefreshData();
    void ClearData();
    void SetAutoRefresh(bool bEnabled);
    bool IsAutoRefreshEnabled() const { return bAutoRefresh; }

private:
    // ========== UI 컴포넌트 ==========

    TSharedPtr<SHktIntentEventList> IntentList;
    TSharedPtr<SHktVMStateList> VMList;
    TSharedPtr<SSearchBox> SearchBox;

    // ========== 필터 옵션 ==========

    bool bShowPendingEvents = true;
    bool bShowProcessingEvents = true;
    bool bShowCompletedEvents = true;
    bool bShowFailedEvents = true;
    bool bShowActiveVMs = true;
    bool bShowCompletedVMs = false;

    // ========== 자동 새로고침 ==========

    bool bAutoRefresh = true;
    float AutoRefreshInterval = 0.1f;
    double LastRefreshTime = 0.0;

    // ========== 검색 / 필터 ==========

    FString CurrentSearchText;
    FString SourceTextFilter;     // Source 필드 contains 매칭
    FString EntityIdTextFilter;   // SubjectId contains 매칭

    // ========== 캐시된 데이터 ==========

    TArray<FHktInsightsIntentEntry> CachedIntentEvents;
    TArray<FHktInsightsVMEntry> CachedActiveVMs;
    TArray<FHktInsightsVMEntry> CachedCompletedVMs;
    FHktInsightsStats CachedStats;

    // ========== UI 생성 헬퍼 ==========

    TSharedRef<SWidget> CreateToolbar();
    TSharedRef<SWidget> CreateFilterPanel();
    TSharedRef<SWidget> CreateStatsPanel();

    // ========== 콜백 ==========

    void OnSearchTextChanged(const FText& NewText);
    void OnSearchTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
    FReply OnClearButtonClicked();
    FReply OnPauseResumeButtonClicked();
    void OnFilterCheckChanged(ECheckBoxState NewState, FString FilterName);
    ECheckBoxState GetFilterCheckState(FString FilterName) const;
    void OnSourceFilterChanged(const FText& NewText);
    void OnEntityIdFilterChanged(const FText& NewText);

    // ========== 필터링 ==========

    bool PassesIntentFilter(const FHktInsightsIntentEntry& Entry) const;
    bool PassesVMFilter(const FHktInsightsVMEntry& Entry) const;
    bool PassesSearchFilter(const FHktInsightsIntentEntry& Entry) const;
    bool PassesSearchFilter(const FHktInsightsVMEntry& Entry) const;
};
