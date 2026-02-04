// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HktRtsMinimapWidget.generated.h"

struct FGeometry;
struct FPointerEvent;

/**
 * 미니맵 클릭을 처리하기 위한 UUserWidget C++ 기반 클래스.
 * UMG 블루프린트에서 이 클래스를 부모로 설정해야 합니다.
 * 클릭 시 좌표를 정규화하여 글로벌 이벤트를 발생시킵니다.
 */
UCLASS()
class HKTPRESENTATION_API UHktRtsMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHktRtsMinimapWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/**
	 * 위젯에서 마우스 버튼을 눌렀을 때 호출되는 네이티브 이벤트입니다.
	 * 블루프린트의 OnMouseButtonDown보다 먼저 호출됩니다.
	 */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
};
