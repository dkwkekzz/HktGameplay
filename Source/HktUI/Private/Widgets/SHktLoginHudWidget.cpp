// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Widgets/SHktLoginHudWidget.h"
#include "HktUIHelpers.h"
#include "HktLoginComponent.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"

void SHktLoginHudWidget::Construct(const FArguments& InArgs)
{
	if (InArgs._LoginWidgetDataAsset)
	{
		DataAsset = TStrongObjectPtr<const UHktWidgetLoginHudDataAsset>(
			const_cast<UHktWidgetLoginHudDataAsset*>(InArgs._LoginWidgetDataAsset));
	}

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.Padding(40.f)
			.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.1f, 0.9f))
			[
				SNew(SBox)
				.WidthOverride(350.f)
				[
					SNew(SVerticalBox)

					// 타이틀
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 20.f)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("HKT Login")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
						.ColorAndOpacity(FLinearColor::White)
					]

					// ID
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("User ID")))
						.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 12.f)
					[
						SAssignNew(IDInputBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Enter ID...")))
					]

					// PW
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 4.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Password")))
						.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 20.f)
					[
						SAssignNew(PWInputBox, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Enter Password...")))
						.IsPassword(true)
					]

					// 로그인 버튼
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(0.f, 0.f, 0.f, 12.f)
					[
						SNew(SBox)
						.WidthOverride(200.f)
						.HeightOverride(40.f)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.OnClicked(this, &SHktLoginHudWidget::OnLoginClicked)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Login")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							]
						]
					]

					// 상태 텍스트
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SAssignNew(StatusText, STextBlock)
						.Text(FText::GetEmpty())
						.ColorAndOpacity(FLinearColor(1.f, 0.4f, 0.4f))
					]
				]
			]
		]
	];
}

FReply SHktLoginHudWidget::OnLoginClicked()
{
	const FString ID = IDInputBox.IsValid() ? IDInputBox->GetText().ToString() : FString();
	const FString PW = PWInputBox.IsValid() ? PWInputBox->GetText().ToString() : FString();

	if (ID.IsEmpty() || PW.IsEmpty())
	{
		if (StatusText.IsValid())
		{
			StatusText->SetText(FText::FromString(TEXT("ID and Password cannot be empty.")));
		}
		return FReply::Handled();
	}

	APlayerController* PC = CachedPC.Get();
	if (!PC)
	{
		PC = HktUI::GetFirstLocalPlayerController();
	}

	if (!PC)
	{
		if (StatusText.IsValid())
		{
			StatusText->SetText(FText::FromString(TEXT("PlayerController not found.")));
		}
		return FReply::Handled();
	}

	// LoginComponent를 통한 로그인 요청
	UHktLoginComponent* LoginComp = HktUI::FindComponent<UHktLoginComponent>(PC);
	if (LoginComp)
	{
		LoginComp->Server_RequestLogin(ID, PW);
		if (StatusText.IsValid())
		{
			StatusText->SetText(FText::FromString(TEXT("Logging in...")));
		}
	}

	return FReply::Handled();
}
