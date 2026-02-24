// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktActorRenderer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

FHktActorRenderer::FHktActorRenderer(ULocalPlayer* InLP)
	: LocalPlayer(InLP)
{
}

void FHktActorRenderer::Sync(const FHktPresentationState& State)
{
	const int64 Frame = State.GetCurrentFrame();

	for (FHktEntityId Id : State.SpawnedThisFrame)
	{
		const FHktEntityPresentation* E = State.Get(Id);
		if (E && E->RenderCategory == EHktRenderCategory::Actor)
			SpawnActor(*E);
	}
	for (FHktEntityId Id : State.RemovedThisFrame)
		DestroyActor(Id);
	for (FHktEntityId Id : State.DirtyThisFrame)
	{
		const FHktEntityPresentation* E = State.Get(Id);
		if (!E || E->RenderCategory != EHktRenderCategory::Actor) continue;
		if (TWeakObjectPtr<AActor>* Found = ActorMap.Find(Id))
			if (Found->IsValid())
				UpdateActor(Found->Get(), *E, Frame);
	}
}

void FHktActorRenderer::Teardown()
{
	ActorMap.Empty();
}

AActor* FHktActorRenderer::GetActor(FHktEntityId Id) const
{
	if (TWeakObjectPtr<AActor> const* P = ActorMap.Find(Id))
		return P->Get();
	return nullptr;
}

void FHktActorRenderer::SpawnActor(const FHktEntityPresentation& Entity)
{
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!World) return;
	// TODO: 스폰 클래스/프리팹 결정 후 SpawnActor, ActorMap.Add(Entity.EntityId, Actor)
	(void)Entity;
}

void FHktActorRenderer::DestroyActor(FHktEntityId Id)
{
	if (TWeakObjectPtr<AActor>* P = ActorMap.Find(Id))
	{
		if (AActor* A = P->Get())
			A->Destroy();
		ActorMap.Remove(Id);
	}
}

void FHktActorRenderer::UpdateActor(AActor* Actor, const FHktEntityPresentation& Entity, int64 Frame)
{
	if (Entity.Transform.Location.IsDirty(Frame) || Entity.Transform.Rotation.IsDirty(Frame))
		Actor->SetActorLocationAndRotation(Entity.Transform.Location.Get(), Entity.Transform.Rotation.Get());
	// TODO: IHktAnimInterface 연동 (AnimState, VisualState)
	(void)Entity;
	(void)Frame;
}
