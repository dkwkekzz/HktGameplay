// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HktRtsHudWidget.generated.h"

struct FGeometry;
struct FPointerEvent;
class AHktRtsUnit;
class UButton;

/**
 * 미니맵 클릭을 처리하기 위한 UUserWidget C++ 기반 클래스.
 * UMG 블루프린트에서 이 클래스를 부모로 설정해야 합니다.
 * 클릭 시 좌표를 정규화하여 PlayerController의 MoveCameraToMinimapLocation를 호출합니다.
 */
UCLASS()
class HKTPRESENTATION_API UHktRtsHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	
	/**
	 * 유닛 생성 버튼 클릭 시 호출됩니다.
	 * NativeConstruct에서 Button_CreateUnit의 OnClicked 이벤트에 바인딩됩니다.
	 */
	UFUNCTION()
	void HandleClickCreateUnit();
	
	/** 스폰할 유닛의 클래스. 블루프린트에서 설정합니다. */
	UPROPERTY(EditDefaultsOnly, Category = "RTS|HUD")
	TSubclassOf<APawn> UnitClassToSpawn;

	/** 유닛 생성 버튼. UMG 에디터에서 이름이 일치하는 위젯과 자동으로 바인딩됩니다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_CreateUnit;
};
