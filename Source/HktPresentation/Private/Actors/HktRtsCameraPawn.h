// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreDefs.h"
#include "GameFramework/SpectatorPawn.h"
#include "HktRtsCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * RTS 스타일 카메라 이동·줌을 담당하는 폰.
 * PlayerController가 이 폰을 Possess합니다.
 * 선택된 유닛이 없을 때 새로 스폰된 엔터티를 자동으로 따라갑니다.
 */
UCLASS()
class HKTPRESENTATION_API AHktRtsCameraPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	AHktRtsCameraPawn();

	/** 마우스 휠 등으로 호출할 줌 처리 */
	void HandleZoom(float Value);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float ZoomSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float MinZoom = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float MaxZoom = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float EdgeScrollThickness = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float CameraScrollSpeed = 3000.0f;

	/** 선택 없을 때 새로 스폰된 엔터티 따라가기 보간 속도 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float FollowInterpSpeed = 5.0f;

private:
	void Zoom(float AxisValue);
	void HandleCameraEdgeScroll(float DeltaTime);
	void UpdateFollowTarget(float DeltaTime);
	void OnSubjectChanged(FHktEntityId EntityId);

	/** 따라갈 엔터티 ID (선택이 없을 때만 유효) */
	FHktEntityId FollowTargetEntityId = InvalidEntityId;
	/** true면 새로 스폰된 엔터티를 따라감 */
	bool bFollowNewSpawn = true;

	FDelegateHandle WheelInputHandle;
	FDelegateHandle SubjectChangedHandle;
	TWeakObjectPtr<class APlayerController> BoundPlayerController;
};
