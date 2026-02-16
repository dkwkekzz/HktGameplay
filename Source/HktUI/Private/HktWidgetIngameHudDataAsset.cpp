// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWidgetIngameHudDataAsset.h"
#include "HktSlateView.h"
#include "IHktUIView.h"
#include "Widgets/SHktIngameHudWidget.h"

TSharedPtr<IHktUIView> UHktWidgetIngameHudDataAsset::CreateView() const
{
	TSharedRef<SHktIngameHudWidget> Widget = SNew(SHktIngameHudWidget);
	return MakeShared<FHktSlateView>(Widget);
}
