// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktUIAnchorStrategy.h"
#include "HktCoreDefs.h"
#include "HktWorldViewAnchorStrategy.generated.h"

class APlayerController;

/**
 * 엔티티의 월드 위치를 스크린 좌표로 투영하는 전략.
 * PresentationState의 Location으로부터 SetWorldPosition()을 통해 위치를 갱신합니다.
 */
UCLASS(BlueprintType)
class HKTUI_API UHktWorldViewAnchorStrategy : public UHktUIAnchorStrategy
{
	GENERATED_BODY()

public:
	void SetTargetEntity(FHktEntityId InEntityId, FVector InWorldOffset = FVector(0.f, 0.f, 100.f))
	{
		TargetEntityId = InEntityId;
		WorldOffset = InWorldOffset;
	}

	/** PresentationState의 Entity Location으로 월드 위치를 갱신 */
	void SetWorldPosition(const FVector& InWorldPosition)
	{
		CachedWorldPosition = InWorldPosition;
		bHasWorldPosition = true;
	}

	FHktEntityId GetTargetEntityId() const { return TargetEntityId; }

	virtual bool CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos) override;

private:
	FHktEntityId TargetEntityId = InvalidEntityId;
	FVector CachedWorldPosition = FVector::ZeroVector;
	FVector WorldOffset = FVector(0.f, 0.f, 100.f);
	bool bHasWorldPosition = false;
};
