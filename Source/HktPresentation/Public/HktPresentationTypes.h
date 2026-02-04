// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "HktPresentationTypes.generated.h"

/**
 * HUD 표시 데이터
 */
USTRUCT(BlueprintType)
struct HKTPRESENTATION_API FHktEntityHUDData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FHktEntityId EntityId;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 Health = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 MaxHealth = 100;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 Mana = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 MaxMana = 100;

	float GetHealthPercent() const 
	{ 
		return MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 0.0f; 
	}

	float GetManaPercent() const 
	{ 
		return MaxMana > 0 ? static_cast<float>(Mana) / MaxMana : 0.0f; 
	}
};

/**
 * 카메라 설정
 */
USTRUCT(BlueprintType)
struct HKTPRESENTATION_API FHktCameraConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom")
	float MinZoomDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom")
	float MaxZoomDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom")
	float ZoomSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zoom")
	float ZoomInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveInterpSpeed = 5.0f;
};
