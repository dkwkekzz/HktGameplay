// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "HktInsightsTypes.h"

/**
 * Intent 이벤트 목록 위젯
 * 
 * Intent 이벤트를 테이블 형태로 표시합니다.
 */
class HKTINSIGHTS_API SHktIntentEventList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktIntentEventList) {}
    SLATE_END_ARGS()

    /** 위젯 생성 */
    void Construct(const FArguments& InArgs);

    /** 아이템 목록 설정 */
    void SetItems(const TArray<FHktInsightsIntentEntry>& Items);

    /** 아이템 목록 반환 */
    const TArray<TSharedPtr<FHktInsightsIntentEntry>>& GetItems() const { return ListItems; }

    /** 선택된 아이템 반환 */
    TSharedPtr<FHktInsightsIntentEntry> GetSelectedItem() const;

    /** 선택 변경 델리게이트 */
    DECLARE_DELEGATE_OneParam(FOnIntentSelected, TSharedPtr<FHktInsightsIntentEntry>);
    FOnIntentSelected OnIntentSelected;

private:
    /** 리스트 뷰 */
    TSharedPtr<SListView<TSharedPtr<FHktInsightsIntentEntry>>> ListView;

    /** 아이템 목록 */
    TArray<TSharedPtr<FHktInsightsIntentEntry>> ListItems;

    /** 행 생성 콜백 */
    TSharedRef<ITableRow> GenerateRow(
        TSharedPtr<FHktInsightsIntentEntry> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    /** 선택 변경 콜백 */
    void OnSelectionChanged(
        TSharedPtr<FHktInsightsIntentEntry> SelectedItem,
        ESelectInfo::Type SelectInfo);

    /** 헤더 행 생성 */
    TSharedRef<SWidget> CreateHeaderRow();
};

/**
 * Intent 이벤트 행 위젯
 */
class SHktIntentEventRow : public SMultiColumnTableRow<TSharedPtr<FHktInsightsIntentEntry>>
{
public:
    SLATE_BEGIN_ARGS(SHktIntentEventRow) {}
        SLATE_ARGUMENT(TSharedPtr<FHktInsightsIntentEntry>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable);

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FHktInsightsIntentEntry> Item;
};
