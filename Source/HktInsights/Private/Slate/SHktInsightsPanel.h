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
 * HKT 디버그 메인 패널 위젯
 * 
 * Intent 이벤트 목록과 VM 상태 목록을 표시합니다.
 * 필터링, 검색, 자동 새로고침 기능을 제공합니다.
 */
class HKTINSIGHTS_API SHktInsightsPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktInsightsPanel)
        : _AutoRefreshInterval(0.1f)  // 100ms
    {}
        /** 자동 새로고침 간격 (초) */
        SLATE_ARGUMENT(float, AutoRefreshInterval)
    SLATE_END_ARGS()

    /** 위젯 생성 */
    void Construct(const FArguments& InArgs);

    /** Tick 오버라이드 - 자동 새로고침 */
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

    /** 데이터 새로고침 */
    void RefreshData();

    /** 모든 데이터 클리어 */
    void ClearData();

    /** 자동 새로고침 활성화/비활성화 */
    void SetAutoRefresh(bool bEnabled);

    /** 자동 새로고침 상태 확인 */
    bool IsAutoRefreshEnabled() const { return bAutoRefresh; }

private:
    // ========== UI 컴포넌트 ==========

    /** Intent 이벤트 목록 */
    TSharedPtr<SHktIntentEventList> IntentList;

    /** VM 상태 목록 */
    TSharedPtr<SHktVMStateList> VMList;

    /** 검색 박스 */
    TSharedPtr<SSearchBox> SearchBox;

    // ========== 필터 옵션 ==========

    /** Pending 이벤트 표시 */
    bool bShowPendingEvents = true;

    /** Processing 이벤트 표시 */
    bool bShowProcessingEvents = true;

    /** Completed 이벤트 표시 */
    bool bShowCompletedEvents = true;

    /** Failed 이벤트 표시 */
    bool bShowFailedEvents = true;

    /** Active VM 표시 */
    bool bShowActiveVMs = true;

    /** Completed VM 표시 */
    bool bShowCompletedVMs = false;

    // ========== 자동 새로고침 ==========

    /** 자동 새로고침 활성화 */
    bool bAutoRefresh = true;

    /** 자동 새로고침 간격 */
    float AutoRefreshInterval = 0.1f;

    /** 마지막 새로고침 시간 */
    double LastRefreshTime = 0.0;

    // ========== 검색 ==========

    /** 현재 검색어 */
    FString CurrentSearchText;

    // ========== 캐시된 데이터 ==========

    /** 캐시된 Intent 이벤트 */
    TArray<FHktInsightsIntentEntry> CachedIntentEvents;

    /** 캐시된 활성 VM */
    TArray<FHktInsightsVMEntry> CachedActiveVMs;

    /** 캐시된 완료 VM */
    TArray<FHktInsightsVMEntry> CachedCompletedVMs;

    /** 캐시된 통계 */
    FHktInsightsStats CachedStats;

    // ========== UI 생성 헬퍼 ==========

    /** 툴바 생성 */
    TSharedRef<SWidget> CreateToolbar();

    /** 필터 패널 생성 */
    TSharedRef<SWidget> CreateFilterPanel();

    /** 통계 패널 생성 */
    TSharedRef<SWidget> CreateStatsPanel();

    // ========== 콜백 ==========

    /** 검색어 변경 콜백 */
    void OnSearchTextChanged(const FText& NewText);

    /** 검색 제출 콜백 */
    void OnSearchTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

    /** Clear 버튼 클릭 */
    FReply OnClearButtonClicked();

    /** Pause/Resume 버튼 클릭 */
    FReply OnPauseResumeButtonClicked();

    /** 필터 체크박스 변경 콜백 */
    void OnFilterCheckChanged(ECheckBoxState NewState, FString FilterName);

    /** 필터 체크 상태 반환 */
    ECheckBoxState GetFilterCheckState(FString FilterName) const;

    // ========== 필터링 ==========

    /** Intent 이벤트 필터 적용 */
    bool PassesIntentFilter(const FHktInsightsIntentEntry& Entry) const;

    /** VM 필터 적용 */
    bool PassesVMFilter(const FHktInsightsVMEntry& Entry) const;

    /** 검색 필터 적용 (Intent) */
    bool PassesSearchFilter(const FHktInsightsIntentEntry& Entry) const;

    /** 검색 필터 적용 (VM) */
    bool PassesSearchFilter(const FHktInsightsVMEntry& Entry) const;
};
