// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWidgetLoginHudDataAsset.h"
#include "HktSlateView.h"
#include "IHktUIView.h"
#include "Widgets/SHktLoginHudWidget.h"
#include "Engine/Texture2D.h"

TSharedPtr<IHktUIView> UHktWidgetLoginHudDataAsset::CreateView() const
{
	TSharedRef<SHktLoginHudWidget> Widget = SNew(SHktLoginHudWidget)
		.LoginWidgetDataAsset(this);

	return MakeShared<FHktSlateView>(Widget);
}
