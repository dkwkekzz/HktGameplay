// Fill out your copyright notice in the Description page of Project Settings.


#include "HktRtsHudWidget.h"
//#include "HktGlobalEventSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "Components/Button.h"

void UHktRtsHudWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_CreateUnit)
	{
		Button_CreateUnit->OnClicked.AddDynamic(this, &UHktRtsHudWidget::HandleClickCreateUnit);
	}
}

void UHktRtsHudWidget::HandleClickCreateUnit()
{
	if (UWorld* World = GetWorld())
	{
		// 변경된 이름의 서브시스템을 가져옵니다.
		//if (UHktGlobalEventSubsystem* GlobalEvents = World->GetSubsystem<UHktGlobalEventSubsystem>())
		//{
		//	GlobalEvents->OnUnitSpawnRequest.Broadcast(UnitClassToSpawn);
		//}
	}
}
