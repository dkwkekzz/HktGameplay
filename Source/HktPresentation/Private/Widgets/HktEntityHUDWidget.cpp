// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktEntityHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UHktEntityHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기 가시성 설정
	if (EntityIdText)
	{
		EntityIdText->SetVisibility(bShowEntityId ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (HealthBar)
	{
		HealthBar->SetVisibility(bShowHealthBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (ManaBar)
	{
		ManaBar->SetVisibility(bShowManaBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UHktEntityHUDWidget::UpdateHUDData(const FHktEntityHUDData& Data)
{
	CachedData = Data;

	// 체력바 업데이트
	if (HealthBar && bShowHealthBar)
	{
		HealthBar->SetPercent(Data.GetHealthPercent());
	}

	// 마나바 업데이트
	if (ManaBar && bShowManaBar)
	{
		ManaBar->SetPercent(Data.GetManaPercent());
	}

	// EntityId 텍스트 업데이트
	if (EntityIdText && bShowEntityId)
	{
		EntityIdText->SetText(FText::AsNumber(Data.EntityId.RawValue));
	}

	// Blueprint 이벤트 호출
	OnHUDDataUpdated(Data);
}

void UHktEntityHUDWidget::SetShowEntityId(bool bShow)
{
	bShowEntityId = bShow;

	if (EntityIdText)
	{
		EntityIdText->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UHktEntityHUDWidget::SetShowHealthBar(bool bShow)
{
	bShowHealthBar = bShow;

	if (HealthBar)
	{
		HealthBar->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UHktEntityHUDWidget::SetShowManaBar(bool bShow)
{
	bShowManaBar = bShow;

	if (ManaBar)
	{
		ManaBar->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
