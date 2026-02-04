// Fill out your copyright notice in the Description page of Project Settings.


#include "HktRtsMinimapWidget.h"
#include "Input/Events.h"        // For FReply, FPointerEvent
#include "Layout/Geometry.h"     // For FGeometry
#include "InputCoreTypes.h"      // For EKeys
#include "Kismet/GameplayStatics.h"
//#include "HktGlobalEventSubsystem.h"
#include "Engine/World.h"

UHktRtsMinimapWidget::UHktRtsMinimapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 위젯이 클릭에 반응하도록 설정
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

FReply UHktRtsMinimapWidget::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseButtonDown(MyGeometry, MouseEvent);

	// 좌클릭일 때만 반응
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// 1. 클릭된 위치를 위젯의 로컬 좌표로 변환 (AbsoluteToLocal)
		FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

		// 2. 위젯의 전체 크기 가져오기
		FVector2D WidgetSize = MyGeometry.GetLocalSize();

		if (WidgetSize.X <= 0 || WidgetSize.Y <= 0)
		{
			return FReply::Handled(); // 위젯 크기가 0이면 무시
		}

		// 3. 정규화된 좌표 계산 (0.0 ~ 1.0)
		// 로컬 좌표가 위젯 범위를 벗어날 수도 있지만, 로직상 (0,0)-(Size,Size)로 Clamp
		float NormalizedX = FMath::Clamp(LocalPosition.X / WidgetSize.X, 0.0f, 1.0f);
		float NormalizedY = FMath::Clamp(LocalPosition.Y / WidgetSize.Y, 0.0f, 1.0f);

		FVector2D NormalizedLocation(NormalizedX, NormalizedY);

		// 4. Get the Event Subsystem and broadcast the event
		if (UWorld* World = GetWorld())
		{
			//if (UHktGlobalEventSubsystem* EventSubsystem = World->GetSubsystem<UHktGlobalEventSubsystem>())
			//{
			//	EventSubsystem->OnMinimapCameraMoveRequest.Broadcast(NormalizedLocation);
			//}
		}

		// 6. 클릭 이벤트를 처리했음을 반환 (이벤트 버블링 중단)
		return FReply::Handled();
	}

	// 그 외의 클릭(우클릭 등)은 처리 안 함
	return FReply::Unhandled();
}
