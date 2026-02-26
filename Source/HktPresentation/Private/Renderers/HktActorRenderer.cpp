// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktActorRenderer.h"
#include "HktActorVisualDataAsset.h"
#include "HktAssetSubsystem.h"
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
	if (!Entity.Visualization.VisualElement.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktActorRenderer] SpawnActor: Entity %d has no VisualElement tag"), Entity.EntityId);
		return;
	}

	UHktAssetSubsystem* AssetSubsystem = UHktAssetSubsystem::Get(World);
	if (!AssetSubsystem) return;

	UHktTagDataAsset* LoadedAsset = AssetSubsystem->LoadAssetSync(Entity.Visualization.VisualElement);
	UHktActorVisualDataAsset* VisualAsset = Cast<UHktActorVisualDataAsset>(LoadedAsset);
	if (!VisualAsset || !VisualAsset->ActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktActorRenderer] SpawnActor: No UHktActorVisualDataAsset or ActorClass for tag %s"), *Entity.Visualization.VisualElement.ToString());
		return;
	}

	FVector Location = Entity.Transform.Location.Get();
	FRotator Rotation = Entity.Transform.Rotation.Get();
	FActorSpawnParameters SpawnParams;
	AActor* SpawnedActor = World->SpawnActor<AActor>(VisualAsset->ActorClass, Location, Rotation, SpawnParams);
	if (SpawnedActor)
	{
		ActorMap.Add(Entity.EntityId, SpawnedActor);
	}
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
