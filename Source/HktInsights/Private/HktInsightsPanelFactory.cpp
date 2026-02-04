// Copyright HKT. All Rights Reserved.

#include "HktInsightsPanelFactory.h"
#include "Slate/SHktInsightsPanel.h"

TSharedRef<SWidget> FHktInsightsPanelFactory::CreatePanel()
{
    return SNew(SHktInsightsPanel);
}

TSharedRef<SWidget> FHktInsightsPanelFactory::CreatePanel(float AutoRefreshInterval)
{
    return SNew(SHktInsightsPanel)
        .AutoRefreshInterval(AutoRefreshInterval);
}
