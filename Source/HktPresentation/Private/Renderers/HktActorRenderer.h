// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktPresentationRenderer.h"
#include "HktPresentationState.h"

class ULocalPlayer;

/** 엔티티별 비주얼 이동 상태 (단순 보간) */
struct FHktActorMotionState
{
	FVector TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;
	bool bIsMoving = false;
	bool bNeedsGroundSnap = true;
};

class FHktActorRenderer : public IHktPresentationRenderer
{
public:
	explicit FHktActorRenderer(ULocalPlayer* InLP);
	virtual void Sync(const FHktPresentationState& State) override;
	virtual void Teardown() override;

	AActor* GetActor(FHktEntityId Id) const;

private:
	void SpawnActor(const FHktEntityPresentation& Entity);
	void DestroyActor(FHktEntityId Id);
	void UpdateMotionTarget(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame);
	void InterpolateActors(float DeltaSeconds);

	/** 지면 높이 트레이스 (위에서 아래로 라인트레이스) */
	bool TraceGroundZ(UWorld* World, const FVector& Pos, float& OutZ) const;

	TMap<FHktEntityId, TWeakObjectPtr<AActor>> ActorMap;
	TMap<FHktEntityId, FHktActorMotionState> MotionStates;
	ULocalPlayer* LocalPlayer = nullptr;

	static constexpr float LerpAlpha = 0.5f;          // 매 프레임 50% 접근 → ~2틱에 도달
	static constexpr float SnapDistance = 1.0f;        // cm, 이 거리 이내면 스냅
	static constexpr float TraceHalfHeight = 500.0f;
};
