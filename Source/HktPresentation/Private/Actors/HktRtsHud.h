// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HktRtsHud.generated.h"

class UUserWidget;

/**
 * RTS HUD 클래스
 * 미니맵, 명령 버튼 등을 포함하는 메인 UMG 위젯을 생성하고 관리합니다.
 */
UCLASS()
class HKTPRESENTATION_API AHktRtsHud : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	/** 게임 시작 시 생성할 메인 RTS UI 위젯 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RTS|UI")
	TSubclassOf<UUserWidget> MainRtsWidgetClass;

private:
	/** 생성된 메인 RTS UI 위젯 인스턴스 */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainRtsWidget;
};
