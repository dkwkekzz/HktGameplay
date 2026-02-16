// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWidgetEntityHudDataAsset.h"
#include "HktSlateView.h"
#include "IHktUIView.h"
#include "Widgets/SHktEntityHudWidget.h"

TSharedPtr<IHktUIView> UHktWidgetEntityHudDataAsset::CreateView() const
{
	TSharedRef<SHktEntityHudWidget> Widget = SNew(SHktEntityHudWidget);
	return MakeShared<FHktSlateView>(Widget);
}
