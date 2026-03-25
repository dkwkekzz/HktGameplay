// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktPresentationRenderer.h"
#include "HktPresentationState.h"

class ULocalPlayer;

class FHktActorRenderer : public IHktPresentationRenderer
{
public:
	explicit FHktActorRenderer(ULocalPlayer* InLP);
	virtual void Sync(const FHktPresentationState& State) override;
	virtual void Teardown() override;
	virtual bool NeedsTick() const override { return !ActorMap.IsEmpty(); }

	AActor* GetActor(FHktEntityId Id) const;

private:
	void SpawnActor(const FHktEntityPresentation& Entity);
	void DestroyActor(FHktEntityId Id);
	void UpdateAnimation(FHktEntityId Id, const FHktEntityPresentation& Entity, int64 Frame, bool bForceUpdate = false);
	void ApplyTransform(FHktEntityId Id, const FHktEntityPresentation& Entity);

	/** ViewModel에서 직접 읽어 actor 즉시 초기화 */
	void InitActorFromPresentation(AActor* Actor, FHktEntityId Id, const FHktEntityPresentation& Entity);

	/** 아이템 Actor를 소유 캐릭터의 소켓에 직접 부착 시도 */
	void TryAttachToOwnerDirect(FHktEntityId ItemId, FHktEntityId OwnerId);
	/** Owner 스폰 시 ViewModel 기반으로 부착 대기 아이템 검색 및 부착 */
	void AttachPendingItemsForOwner(FHktEntityId OwnerEntityId);
	/** 소켓에서 분리하여 독립 Actor로 복원 */
	void DetachFromOwner(FHktEntityId ItemId);

	TMap<FHktEntityId, TWeakObjectPtr<AActor>> ActorMap;
	TSet<FHktEntityId> AttachedItems;
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;

	/** Sync마다 갱신 — async callback에서 ViewModel 직접 조회용 */
	const FHktPresentationState* CachedState = nullptr;

	/** 비동기 콜백에서 this 유효성 확인용 (Teardown 시 리셋) */
	TSharedPtr<bool> AliveGuard = MakeShared<bool>(true);
};
