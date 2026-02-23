// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "HktInsightsRuntimeTypes.h"

class SSearchBox;

/** TPair<FString,FString>의 콤마가 SLATE_ARGUMENT 매크로를 오해하지 않도록 alias 사용 */
using FHktPropPair = TPair<FString, FString>;

/**
 * FHktEntityDisplayRow - 엔티티 리스트뷰 1행 데이터
 *
 * SHktWorldStatePanel 내부에서만 사용하며, SListView의 아이템 타입으로 쓰입니다.
 */
struct FHktEntityDisplayRow
{
    /** 소스 이름 ("Server[0]", "Client" 등) */
    FString Source;

    /** 엔티티 ID */
    int32 EntityId = -1;

    /** 타입 이름 ("Unit", "Projectile" 등) */
    FString TypeName;

    /** 핵심 프로퍼티 요약 (UI 표시용) */
    FString PropSummary;

    /** 전체 프로퍼티 (이름/값 쌍) - 상세 패널 표시용 */
    TArray<FHktPropPair> Properties;
};

/**
 * SHktWorldStatePanel - 서버/클라이언트 WorldState 시각화 패널
 *
 * 레이아웃:
 *   ┌─ 툴바 (검색 | Pause | Clear) ─────────────────────────┐
 *   │                                                         │
 *   ├─ 엔티티 목록 (Source | EntityId | Type | Properties) ──┤
 *   │                                                         │
 *   ├─ 선택된 엔티티 상세 (PropName | Value) ────────────────┤
 *   │                                                         │
 *   └─ 통계 바 (Sources: N | Entities: M | Frame 정보) ──────┘
 */
class HKTINSIGHTS_API SHktWorldStatePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktWorldStatePanel)
        : _AutoRefreshInterval(0.1f)
    {}
        /** 자동 새로고침 간격 (초) */
        SLATE_ARGUMENT(float, AutoRefreshInterval)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
    // ========== UI 생성 ==========

    TSharedRef<SWidget> CreateToolbar();
    TSharedRef<SWidget> CreateEntitySection();
    TSharedRef<SWidget> CreateDetailSection();
    TSharedRef<SWidget> CreateStatsBar();

    // ========== 엔티티 리스트 ==========

    TSharedRef<ITableRow> GenerateEntityRow(
        TSharedPtr<FHktEntityDisplayRow> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    void OnEntitySelected(TSharedPtr<FHktEntityDisplayRow> Item, ESelectInfo::Type SelectType);

    TSharedPtr<SListView<TSharedPtr<FHktEntityDisplayRow>>> EntityListView;
    TArray<TSharedPtr<FHktEntityDisplayRow>>                EntityListItems;

    // ========== 프로퍼티 상세 리스트 ==========

    TSharedRef<ITableRow> GenerateDetailRow(
        TSharedPtr<FHktPropPair> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    void UpdateDetailPanel(const FHktEntityDisplayRow& Row);

    TSharedPtr<SListView<TSharedPtr<FHktPropPair>>> DetailListView;
    TArray<TSharedPtr<FHktPropPair>>                DetailListItems;

    // ========== 데이터 갱신 ==========

    void RefreshData();

    TArray<FHktWorldStateSnapshot> CachedSnapshots;
    int32 CachedTotalEntities = 0;
    int32 CachedNumSources    = 0;

    // ========== 자동 새로고침 ==========

    bool  bAutoRefresh        = true;
    float AutoRefreshInterval = 0.1f;
    double LastRefreshTime    = 0.0;

    // ========== 검색 ==========

    FString SearchText;

    // ========== 콜백 ==========

    void    OnSearchTextChanged(const FText& NewText);
    FReply  OnPauseResumeClicked();
    FReply  OnClearClicked();

    TSharedPtr<SSearchBox> SearchBox;
};
