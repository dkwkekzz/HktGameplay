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

/** 스폰 시점 ViewModel 스냅샷 — async 콜백에서 actor 초기화에 사용 */
struct FHktSpawnSnapshot
{
	FVector Location;
	FRotator Rotation;
	bool bIsMoving = false;
	FVector Velocity = FVector::ZeroVector;
	FGameplayTag Stance;
	FGameplayTagContainer Tags;
	float AttackSpeed = 0.f;
	float CPRatio = 0.f;
	int32 OwnerEntity = 0;
	int32 ItemState = 0;
};

class FHktActorRenderer : public IHktPresentationRenderer
{
public:
	explicit FHktActorRenderer(ULocalPlayer* InLP);
	virtual void Sync(const FHktPresentationState& State) override;
	virtual void Teardown() override;
	virtual bool NeedsTick() const override { return !MotionStates.IsEmpty(); }

	AActor* GetActor(FHktEntityId Id) const;

private:
	void SpawnActor(const FHktEntityPresentation& Entity);
	void DestroyActor(FHktEntityId Id);
	void UpdateMotionTarget(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame, bool bForceUpdate = false);
	void UpdateAnimation(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame, bool bForceUpdate = false);
	void InterpolateActors(float DeltaSeconds);

	/** 지면 높이 트레이스 (위에서 아래로 라인트레이스) */
	bool TraceGroundZ(UWorld* World, const FVector& Pos, float& OutZ) const;

	/** async 콜백에서 스냅샷으로 actor 즉시 초기화 (motion + animation) */
	void InitActorFromSnapshot(AActor* Actor, FHktEntityId Id, const FHktSpawnSnapshot& Snap);

	/** 아이템 Actor를 소유 캐릭터의 소켓에 직접 부착 시도 (실패 시 PendingItemsByOwner에 등록) */
	void TryAttachToOwnerDirect(FHktEntityId ItemId, FHktEntityId OwnerId);
	/** Owner 스폰 시 대기 중인 아이템들 부착 */
	void ProcessPendingAttachmentsForOwner(FHktEntityId OwnerEntityId);
	/** 소켓에서 분리하여 독립 Actor로 복원 */
	void DetachFromOwner(FHktEntityId ItemId);

	TMap<FHktEntityId, TWeakObjectPtr<AActor>> ActorMap;
	TMap<FHktEntityId, FHktActorMotionState> MotionStates;
	TSet<FHktEntityId> AttachedItems;
	TMultiMap<FHktEntityId, FHktEntityId> PendingItemsByOwner;  // Owner → 대기 아이템들
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;

	/** 비동기 콜백에서 this 유효성 확인용 (Teardown 시 리셋) */
	TSharedPtr<bool> AliveGuard = MakeShared<bool>(true);

	static constexpr float LerpAlpha = 0.9f;          // 매 프레임 50% 접근 → ~2틱에 도달
	static constexpr float SnapDistance = 1.0f;        // cm, 이 거리 이내면 스냅
	static constexpr float TraceHalfHeight = 500.0f;
};
