// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "HktInsightsTypes.h"

/**
 * VM 상태 목록 위젯
 * 
 * 활성/완료된 VM을 테이블 형태로 표시합니다.
 */
class HKTINSIGHTS_API SHktVMStateList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktVMStateList) {}
    SLATE_END_ARGS()

    /** 위젯 생성 */
    void Construct(const FArguments& InArgs);

    /** 아이템 목록 설정 */
    void SetItems(const TArray<FHktInsightsVMEntry>& Items);

    /** 아이템 목록 반환 */
    const TArray<TSharedPtr<FHktInsightsVMEntry>>& GetItems() const { return ListItems; }

    /** 선택된 아이템 반환 */
    TSharedPtr<FHktInsightsVMEntry> GetSelectedItem() const;

    /** 선택 변경 델리게이트 */
    DECLARE_DELEGATE_OneParam(FOnVMSelected, TSharedPtr<FHktInsightsVMEntry>);
    FOnVMSelected OnVMSelected;

private:
    /** 리스트 뷰 */
    TSharedPtr<SListView<TSharedPtr<FHktInsightsVMEntry>>> ListView;

    /** 아이템 목록 */
    TArray<TSharedPtr<FHktInsightsVMEntry>> ListItems;

    /** 행 생성 콜백 */
    TSharedRef<ITableRow> GenerateRow(
        TSharedPtr<FHktInsightsVMEntry> Item,
        const TSharedRef<STableViewBase>& OwnerTable);

    /** 선택 변경 콜백 */
    void OnSelectionChanged(
        TSharedPtr<FHktInsightsVMEntry> SelectedItem,
        ESelectInfo::Type SelectInfo);
};

/**
 * VM 상태 행 위젯
 */
class SHktVMStateRow : public SMultiColumnTableRow<TSharedPtr<FHktInsightsVMEntry>>
{
public:
    SLATE_BEGIN_ARGS(SHktVMStateRow) {}
        SLATE_ARGUMENT(TSharedPtr<FHktInsightsVMEntry>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable);

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
    TSharedPtr<FHktInsightsVMEntry> Item;

    /** 진행률 바 위젯 생성 */
    TSharedRef<SWidget> CreateProgressBar();
};
