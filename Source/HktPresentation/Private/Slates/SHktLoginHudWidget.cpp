// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Slates/SHktLoginHudWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "DataAssets/HktWidgetLoginHudDataAsset.h"

#define LOCTEXT_NAMESPACE "HktLogin"

void SHktLoginHudWidget::Construct(const FArguments& InArgs)
{
	OnLoginRequested = InArgs._OnLoginRequested;
	CachedBackgroundBrush = InArgs._BackgroundBrush;

	UHktWidgetLoginHudDataAsset* DataAsset = InArgs._LoginWidgetDataAsset;
	if (DataAsset)
	{
		ApplyVideoFromDataAsset(DataAsset);
	}

	TSharedRef<SBox> LoginPanel = SNew(SBox)
		.WidthOverride(400.0f)
		.HeightOverride(280.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 20.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LoginTitle", "Login"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24))
				.Justification(ETextJustify::Center)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 12.0f)
			[
				SAssignNew(IdTextBox, SEditableTextBox)
				.HintText(LOCTEXT("IdHint", "User ID"))
				.IsPassword(false)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 12.0f)
			[
				SAssignNew(PasswordTextBox, SEditableTextBox)
				.HintText(LOCTEXT("PasswordHint", "Password"))
				.IsPassword(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 24.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("LoginButton", "Login"))
				.OnClicked(this, &SHktLoginHudWidget::OnLoginClicked)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
			]
		];

	if (MediaPlayer.IsValid() && MediaTexture.IsValid())
	{
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(&VideoBrush)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				LoginPanel
			]
		];
	}
	else if (CachedBackgroundBrush.IsSet())
	{
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(&CachedBackgroundBrush.GetValue())
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				LoginPanel
			]
		];
	}
	else
	{
		ChildSlot
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.95f))
			.Padding(0.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				LoginPanel
			]
		];
	}
}

void SHktLoginHudWidget::ApplyVideoFromDataAsset(UHktWidgetLoginHudDataAsset* DataAsset)
{
	if (!DataAsset) { return; }

	UMediaPlayer* Player = DataAsset->MediaPlayer;
	UMediaTexture* Texture = DataAsset->MediaTexture;
	UFileMediaSource* Source = DataAsset->MediaSource;
	if (!Player || !Texture) { return; }

	MediaPlayer.Reset(Player);
	MediaTexture.Reset(Texture);
	MediaSource.Reset(Source);

	VideoBrush.SetResourceObject(MediaTexture.Get());
	const float W = MediaTexture->GetWidth();
	const float H = MediaTexture->GetHeight();
	VideoBrush.ImageSize = (W > 0 && H > 0) ? FVector2D(W, H) : FVector2D(1920.0f, 1080.0f);
	VideoBrush.DrawAs = ESlateBrushDrawType::Image;
	VideoBrush.TintColor = FLinearColor::White;
	if (MediaSource.IsValid())
	{
		MediaPlayer->OpenSource(MediaSource.Get());
	}
}

FReply SHktLoginHudWidget::OnLoginClicked()
{
	FString Id = IdTextBox.IsValid() ? IdTextBox->GetText().ToString() : FString();
	FString Pw = PasswordTextBox.IsValid() ? PasswordTextBox->GetText().ToString() : FString();
	OnLoginRequested.ExecuteIfBound(Id, Pw);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
