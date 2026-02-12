// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "Styling/SlateBrush.h"

class SEditableTextBox;
class UHktWidgetLoginHudDataAsset;

DECLARE_DELEGATE_TwoParams(FOnHktLoginRequested, const FString&, const FString&);

class HKTUI_API SHktLoginHudWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHktLoginHudWidget) {}
		SLATE_EVENT(FOnHktLoginRequested, OnLoginRequested)
		SLATE_ARGUMENT(TOptional<FSlateBrush>, BackgroundBrush)
		SLATE_ARGUMENT(UHktWidgetLoginHudDataAsset*, LoginWidgetDataAsset)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnLoginClicked();
	void ApplyVideoFromDataAsset(UHktWidgetLoginHudDataAsset* DataAsset);

	TSharedPtr<SEditableTextBox> IdTextBox;
	TSharedPtr<SEditableTextBox> PasswordTextBox;
	FOnHktLoginRequested OnLoginRequested;
	TOptional<FSlateBrush> CachedBackgroundBrush;
	FSlateBrush VideoBrush;
	TStrongObjectPtr<UMediaPlayer> MediaPlayer;
	TStrongObjectPtr<UMediaTexture> MediaTexture;
	TStrongObjectPtr<UFileMediaSource> MediaSource;
};
