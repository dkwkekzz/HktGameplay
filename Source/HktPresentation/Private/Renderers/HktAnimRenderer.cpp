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
	// --- 제거된 엔터티 정리 ---
	for (FHktEntityId Id : State.RemovedThisFrame)
	{
		PrevAnimTagsMap.Remove(Id);
	}

	// --- 처리 대상: 신규 스폰 + Dirty 엔터티 ---
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
			FGameplayTagContainer CurrentAnimTags = E->Tags.Filter(
				FGameplayTagContainer(HktGameplayTags::Anim));

			FGameplayTagContainer& PrevAnimTags = PrevAnimTagsMap.FindOrAdd(Id);

			// 새로 추가된 태그 → play
			for (const FGameplayTag& Tag : CurrentAnimTags)
			{
				if (!PrevAnimTags.HasTagExact(Tag))
				{
					Handler->PlayAnimByTag(Tag);
				}
			}

			// 제거된 태그 → stop
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
	}

	for (FHktEntityId Id : State.DirtyThisFrame)
	{
		ProcessEntity(Id, false);
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
