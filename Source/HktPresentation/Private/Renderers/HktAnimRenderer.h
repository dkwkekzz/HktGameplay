// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreDefs.h"
#include "HktPresentationRenderer.h"

class FHktActorRenderer;
class IHktAnimHandler;

/**
 * 애니메이션 렌더러.
 *
 * Entity의 Anim.* 태그 변화를 중앙에서 diff하고, IHktAnimHandler를 통해
 * 백엔드(AnimInstance, MassEntity 등)에 play/stop을 위임한다.
 * View(Actor)는 flat 값만 set하고, 이 렌더러가 tag 기반 play/stop을 처리.
 */
class FHktAnimRenderer : public IHktPresentationRenderer
{
public:
	explicit FHktAnimRenderer(FHktActorRenderer* InActorRenderer);

	// --- IHktPresentationRenderer ---
	virtual void Sync(const FHktPresentationState& State) override;
	virtual void Teardown() override;

private:
	/** RenderCategory에 따라 적절한 IHktAnimHandler를 획득 */
	IHktAnimHandler* ResolveHandler(FHktEntityId Id, const FHktEntityPresentation& Entity) const;

	FHktActorRenderer* ActorRenderer = nullptr;

	/** 엔터티별 이전 프레임의 Anim.* 태그 (변화 감지용) */
	TMap<FHktEntityId, FGameplayTagContainer> PrevAnimTagsMap;

	/** Sync 내 중복 처리 방지용 (SpawnedThisFrame ∩ DirtyThisFrame) */
	TSet<FHktEntityId> ProcessedThisSync;
};
