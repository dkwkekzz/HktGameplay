// Fill out your copyright notice in the Description page of Project Settings.


#include "HktRtsHud.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

void AHktRtsHud::BeginPlay()
{
	Super::BeginPlay();

	// 서버에는 UI가 필요 없음
	if (!GetOwningPlayerController() || !GetOwningPlayerController()->IsLocalController())
	{
		return;
	}

	// 메인 RTS 위젯 생성 및 뷰포트에 추가
	if (MainRtsWidgetClass)
	{
		MainRtsWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), MainRtsWidgetClass);
		if (MainRtsWidget)
		{
			MainRtsWidget->AddToViewport();
		}
	}
}
