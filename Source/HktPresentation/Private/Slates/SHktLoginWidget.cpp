// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Slates/SHktLoginWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "HktLogin"

void SHktLoginWidget::Construct(const FArguments& InArgs)
{
	OnLoginRequested = InArgs._OnLoginRequested;
	CachedBackgroundBrush = InArgs._BackgroundBrush;

	// 동영상 배경 에셋 로드 (경로는 프로젝트에 맞게 수정)
	static ConstructorHelpers::FObjectFinder<UMediaPlayer> PlayerObj(TEXT("/Game/Movies/MyMediaPlayer"));
	static ConstructorHelpers::FObjectFinder<UMediaTexture> TextureObj(TEXT("/Game/Movies/MyMediaTexture"));
	static ConstructorHelpers::FObjectFinder<UFileMediaSource> SourceObj(TEXT("/Game/Movies/MyFileMediaSource"));

	MediaPlayer.Reset(PlayerObj.Object);
	MediaTexture.Reset(TextureObj.Object);
	MediaSource.Reset(SourceObj.Object);

	if (MediaPlayer.IsValid() && MediaTexture.IsValid())
	{
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
				.OnClicked(this, &SHktLoginWidget::OnLoginClicked)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
			]
		];

	// 동영상 배경이 유효하면 최하단 레이어로 사용, 없으면 정적 Brush 또는 단색 배경
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

FReply SHktLoginWidget::OnLoginClicked()
{
	FString Id = IdTextBox.IsValid() ? IdTextBox->GetText().ToString() : FString();
	FString Pw = PasswordTextBox.IsValid() ? PasswordTextBox->GetText().ToString() : FString();
	OnLoginRequested.ExecuteIfBound(Id, Pw);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
