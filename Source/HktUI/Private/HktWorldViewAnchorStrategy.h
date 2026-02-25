// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktUIAnchorStrategy.h"
#include "HktCoreDefs.h"
#include "HktWorldState.h"
#include "HktCoreProperties.h"
#include "HktWorldViewAnchorStrategy.generated.h"

class APlayerController;

/**
 * FHktWorldState의 엔티티 PosX/Y/Z를 읽어 월드→스크린 투영하는 전략.
 * AActor가 없는 순수 시뮬레이션 엔티티의 위치를 추적합니다.
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

	void SetWorldState(const FHktWorldState* InWorldState)
	{
		WorldStatePtr = InWorldState;
	}

	FHktEntityId GetTargetEntityId() const { return TargetEntityId; }

	virtual bool CalculateScreenPosition(const UObject* WorldContext, FVector2D& OutScreenPos) override;

private:
	FHktEntityId TargetEntityId = InvalidEntityId;
	const FHktWorldState* WorldStatePtr = nullptr;
	FVector WorldOffset = FVector(0.f, 0.f, 100.f);
};
