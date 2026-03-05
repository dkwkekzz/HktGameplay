// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "HktInsightsRuntimeTypes.h"

class SSearchBox;
class SEditableTextBox;

/** TPair<FString,FString>의 콤마가 SLATE_ARGUMENT 매크로를 오해하지 않도록 alias 사용 */
using FHktPropPair = TPair<FString, FString>;

/**
 * SHktWorldStatePanel - 서버/클라이언트 WorldState 시각화 패널
 *
 * 레이아웃:
 *   ┌─ 필터바 (Source | Type | ID | Search | Pause | Clear) ───┐
 *   ├─ 엔티티 목록 (Source | EntityId | Type) ─────────────────┤  65%
 *   ├─ 상세 패널 ──────────────────────────────────────────────┤  35%
 *   │   선택 엔티티별 SExpandableArea (프로퍼티 리스트)         │
 *   └─ 통계 바 ────────────────────────────────────────────────┘
 *
 * 데이터: AdvanceFrame에서 자동 push (CollectAll 호출 불필요)
 * 다중 선택: ESelectionMode::Multi
 */
class HKTINSIGHTS_API SHktWorldStatePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktWorldStatePanel)
        : _AutoRefreshInterval(0.1f)
    {}
        SLATE_ARGUMENT(float, AutoRefreshInterval)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
    // ========== UI 생성 ==========

    TSharedRef<SWidget> CreateFilterBar();
    TSharedRef<SWidget> CreateEntitySection();
    TSharedRef<SWidget> CreateDetailSection();
    TSharedRef<SWidget> CreateStatsBar();

    // ========== 엔티티 리스트 ==========

    TSharedRef<ITableRow> GenerateEntityRow(
        TSharedPtr<FHktEntityListEntry> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    void OnEntitySelectionChanged(TSharedPtr<FHktEntityListEntry> Item, ESelectInfo::Type SelectType);

    TSharedPtr<SListView<TSharedPtr<FHktEntityListEntry>>> EntityListView;
    TArray<TSharedPtr<FHktEntityListEntry>>                EntityListItems;

    // ========== 상세 패널 ==========

    TSharedPtr<SScrollBox> DetailScrollBox;

    /** 선택된 엔티티별 캐시된 상세 데이터 (Text_Lambda에서 참조) */
    TArray<TSharedPtr<FHktSelectedEntityDetail>> CachedDetailEntries;

    /** 위젯 구조 재생성이 필요한지 (선택 변경 / 프로퍼티 수 변경) */
    bool bDetailWidgetsDirty = true;

    /** 상세 데이터 갱신 (매 틱, 위젯 재생성 없음) */
    void UpdateDetailData();

    /** 상세 위젯 구조 재생성 (선택 변경 시에만) */
    void RebuildDetailWidgets();

    // ========== 데이터 갱신 ==========

    void RefreshData(bool bForceRebuild = false);
    void RebuildEntityList();

    int32 CachedEntityListVersion = -1;
    int32 CachedTotalEntities = 0;
    int32 CachedNumSources = 0;

    // ========== 자동 새로고침 ==========

    bool  bAutoRefresh        = true;
    float AutoRefreshInterval = 0.1f;
    double LastRefreshTime    = 0.0;

    // ========== 필터 ==========

    FString SourceFilter;     // Source contains 매칭 (빈 = All)
    FString TypeFilter;       // Type contains 매칭
    FString EntityIdFilter;   // EntityId contains 매칭
    FString SearchText;       // 일반 텍스트 검색 (모든 필드)

    bool PassesFilter(const FHktEntityListEntry& Entry) const;

    // ========== 콜백 ==========

    void    OnSourceFilterChanged(const FText& NewText);
    void    OnTypeFilterChanged(const FText& NewText);
    void    OnEntityIdFilterChanged(const FText& NewText);
    void    OnSearchTextChanged(const FText& NewText);
    FReply  OnPauseResumeClicked();
    FReply  OnClearClicked();
};
