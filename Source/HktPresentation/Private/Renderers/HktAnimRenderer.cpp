// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimRenderer.h"
#include "HktActorRenderer.h"
#include "Actors/IHktPresentableActor.h"
#include "IHktAnimHandler.h"
#include "HktRuntimeTags.h"
#include "HktCoreEventLog.h"
#include "HktPresentationLog.h"

FHktAnimRenderer::FHktAnimRenderer(FHktActorRenderer* InActorRenderer)
	: ActorRenderer(InActorRenderer)
{
}

void FHktAnimRenderer::Sync(const FHktPresentationState& State)
{
	static const FGameplayTagContainer AnimFilterContainer(HktGameplayTags::Anim);

	// --- 제거된 엔터티 정리 ---
	for (FHktEntityId Id : State.RemovedThisFrame)
	{
		PrevAnimTagsMap.Remove(Id);
	}

	// --- 처리 대상 수집 (Spawned + Dirty 중복 방지) ---
	ProcessedThisSync.Reset();

	auto ProcessEntity = [this, &State](FHktEntityId Id, bool bIsSpawned)
	{
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E) return;

		IHktAnimHandler* Handler = ResolveHandler(Id, *E);
		if (!Handler) return;

		const int64 Frame = State.GetCurrentFrame();

		// Tag diff: Anim.* 태그 변화 감지
		if (bIsSpawned || E->TagsDirtyFrame == Frame)
		{
			FGameplayTagContainer CurrentAnimTags = E->Tags.Filter(AnimFilterContainer);
			FGameplayTagContainer& PrevAnimTags = PrevAnimTagsMap.FindOrAdd(Id);

			for (const FGameplayTag& Tag : CurrentAnimTags)
			{
				if (!PrevAnimTags.HasTagExact(Tag))
				{
					Handler->PlayAnimByTag(Tag);
				}
			}

			for (const FGameplayTag& Tag : PrevAnimTags)
			{
				if (!CurrentAnimTags.HasTagExact(Tag))
				{
					Handler->StopAnimByTag(Tag);
				}
			}

			PrevAnimTags = CurrentAnimTags;
		}

		// 일회성 애니메이션 이벤트 소비 (PlayAnim 경유)
		if (E->PendingAnimTriggers.Num() > 0)
		{
			for (const FGameplayTag& AnimTag : E->PendingAnimTriggers)
			{
				Handler->PlayAnimByTag(AnimTag);
			}
			const_cast<FHktEntityPresentation&>(*E).PendingAnimTriggers.Reset();
		}
	};

	for (FHktEntityId Id : State.SpawnedThisFrame)
	{
		ProcessEntity(Id, true);
		ProcessedThisSync.Add(Id);
	}

	for (FHktEntityId Id : State.DirtyThisFrame)
	{
		if (!ProcessedThisSync.Contains(Id))
		{
			ProcessEntity(Id, false);
		}
	}
}

void FHktAnimRenderer::Teardown()
{
	PrevAnimTagsMap.Empty();
	ActorRenderer = nullptr;
}

IHktAnimHandler* FHktAnimRenderer::ResolveHandler(FHktEntityId Id, const FHktEntityPresentation& Entity) const
{
	switch (Entity.RenderCategory)
	{
	case EHktRenderCategory::Actor:
	{
		if (!ActorRenderer) return nullptr;
		AActor* Actor = ActorRenderer->GetActor(Id);
		if (!Actor) return nullptr;
		IHktPresentableActor* P = Cast<IHktPresentableActor>(Actor);
		return P ? P->GetAnimHandler() : nullptr;
	}

	case EHktRenderCategory::MassEntity:
		// TODO: MassEntityRenderer에서 IHktAnimHandler 획득
		return nullptr;

	default:
		return nullptr;
	}
}
