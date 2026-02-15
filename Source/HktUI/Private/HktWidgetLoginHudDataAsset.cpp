// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWidgetLoginHudDataAsset.h"
#include "HktSlateView.h"
#include "IHktUIView.h"
#include "Engine/Texture2D.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"

TSharedPtr<IHktUIView> UHktWidgetLoginHudDataAsset::CreateView() const
{
	// SHktLoginHudWidget 미구현 시 임시로 SBox로 래핑하여 반환
	TSharedRef<SWidget> Placeholder = SNew(SBox).WidthOverride(200.f).HeightOverride(100.f);
	return MakeShared<FHktSlateView>(Placeholder);
}
