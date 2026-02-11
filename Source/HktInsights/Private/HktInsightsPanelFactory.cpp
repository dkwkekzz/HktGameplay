// Copyright HKT. All Rights Reserved.

#include "HktInsightsPanelFactory.h"
#include "Slate/SHktInsightsPanel.h"
#include "Slate/SHktRuntimeInsightsPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// 탭 헤더 스타일의 통합 패널
class SHktInsightsCombinedPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHktInsightsCombinedPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        ChildSlot
        [
            SNew(SVerticalBox)

            // 탭 버튼
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Intent / VM")))
                    .OnClicked_Lambda([this]() { ShowTab(0); return FReply::Handled(); })
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Runtime State")))
                    .OnClicked_Lambda([this]() { ShowTab(1); return FReply::Handled(); })
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSeparator)
            ]

            // 컨텐츠
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(ContentSwitcher, SWidgetSwitcher)

                + SWidgetSwitcher::Slot()
                [
                    SNew(SHktInsightsPanel)
                ]

                + SWidgetSwitcher::Slot()
                [
                    SNew(SHktRuntimeInsightsPanel)
                ]
            ]
        ];
    }

private:
    TSharedPtr<SWidgetSwitcher> ContentSwitcher;

    void ShowTab(int32 Index)
    {
        if (ContentSwitcher.IsValid())
        {
            ContentSwitcher->SetActiveWidgetIndex(Index);
        }
    }
};

// ============================================================================

TSharedRef<SWidget> FHktInsightsPanelFactory::CreatePanel()
{
    return SNew(SHktInsightsPanel);
}

TSharedRef<SWidget> FHktInsightsPanelFactory::CreatePanel(float AutoRefreshInterval)
{
    return SNew(SHktInsightsPanel)
        .AutoRefreshInterval(AutoRefreshInterval);
}

TSharedRef<SWidget> FHktInsightsPanelFactory::CreateRuntimePanel()
{
    return SNew(SHktRuntimeInsightsPanel);
}

TSharedRef<SWidget> FHktInsightsPanelFactory::CreateRuntimePanel(float AutoRefreshInterval)
{
    return SNew(SHktRuntimeInsightsPanel)
        .AutoRefreshInterval(AutoRefreshInterval);
}

TSharedRef<SWidget> FHktInsightsPanelFactory::CreateCombinedPanel()
{
    return SNew(SHktInsightsCombinedPanel);
}
