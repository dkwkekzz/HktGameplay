// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "HktRtsCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * RTS 카메라 이동 및 줌을 담당하는 폰.
 * PlayerController가 이 폰을 Possess합니다.
 */
UCLASS()
class HKTPRESENTATION_API AHktRtsCameraPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	AHktRtsCameraPawn();

	// 줌 인/아웃 기능 (예: 마우스 휠로)
	void HandleZoom(float Value);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	/** 카메라 붐 (각도와 거리 조절) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	/** 실제 카메라 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** 줌 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float ZoomSpeed = 100.0f;

	/** 최소 줌 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float MinZoom = 500.0f;

	/** 최대 줌 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float MaxZoom = 4000.0f;

	/** 화면 가장자리 스크롤 감지 영역 두께 (퍼센트) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float EdgeScrollThickness = 0.05f; // 화면의 5%

	/** 카메라 스크롤 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraScrollSpeed = 3000.0f;

private:
	void Zoom(float AxisValue);
	void HandleCameraEdgeScroll(float DeltaTime);
	void OnMoveCameraToMinimapLocation(const FVector2D& NormalizedLocation);

	/** PresentationSubsystem OnWheelInput 구독 해제용 */
	TWeakObjectPtr<class UHktPresentationSubsystem> PresentationSubsystem;
};
