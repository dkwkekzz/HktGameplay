// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "HktInsightProvider.h"
#include "HktInsightsRuntimeTypes.h"

class SSearchBox;

/**
 * SHktRuntimeInsightsPanel - 런타임 상태 인사이트 패널
 *
 * 3개 섹션으로 구성:
 *   1) Provider 스냅샷: 카테고리별로 그룹핑된 키-값 테이블
 *   2) Packet 트래픽: 최근 패킷 로그 + 실시간 통계
 *   3) 요약 바: 패킷/초, 바이트/초, Provider 수
 */
class HKTINSIGHTS_API SHktRuntimeInsightsPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktRuntimeInsightsPanel)
        : _AutoRefreshInterval(0.1f)
    {}
        SLATE_ARGUMENT(float, AutoRefreshInterval)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
    // ========== UI 생성 ==========
    TSharedRef<SWidget> CreateProviderSection();
    TSharedRef<SWidget> CreatePacketSection();
    TSharedRef<SWidget> CreateStatsBar();

    // ========== Provider 리스트 ==========
    TSharedRef<ITableRow> GenerateProviderRow(
        TSharedPtr<FHktInsightEntry> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    TSharedPtr<SListView<TSharedPtr<FHktInsightEntry>>> ProviderListView;
    TArray<TSharedPtr<FHktInsightEntry>> ProviderListItems;

    // ========== Packet 리스트 ==========
    TSharedRef<ITableRow> GeneratePacketRow(
        TSharedPtr<FHktPacketRecord> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    TSharedPtr<SListView<TSharedPtr<FHktPacketRecord>>> PacketListView;
    TArray<TSharedPtr<FHktPacketRecord>> PacketListItems;

    // ========== 데이터 ==========
    void RefreshData();

    FHktPacketStats CachedStats;
    int32 CachedProviderCount = 0;

    // ========== 설정 ==========
    bool bAutoRefresh = true;
    float AutoRefreshInterval = 0.1f;
    double LastRefreshTime = 0.0;
    FString SearchText;

    // ========== 콜백 ==========
    void OnSearchTextChanged(const FText& NewText);
    FReply OnPauseResumeClicked();
    FReply OnClearClicked();

    TSharedPtr<SSearchBox> SearchBox;
};

// ========== Provider Row 위젯 ==========

class SHktProviderEntryRow : public SMultiColumnTableRow<TSharedPtr<FHktInsightEntry>>
{
public:
    SLATE_BEGIN_ARGS(SHktProviderEntryRow) {}
        SLATE_ARGUMENT(TSharedPtr<FHktInsightEntry>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable);
    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FHktInsightEntry> Item;
};

// ========== Packet Row 위젯 ==========

class SHktPacketRecordRow : public SMultiColumnTableRow<TSharedPtr<FHktPacketRecord>>
{
public:
    SLATE_BEGIN_ARGS(SHktPacketRecordRow) {}
        SLATE_ARGUMENT(TSharedPtr<FHktPacketRecord>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable);
    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FHktPacketRecord> Item;
};
