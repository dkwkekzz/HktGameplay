// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPresentationSubsystem.h"
#include "IHktPlayerInteractionInterface.h"
#include "Renderers/HktActorRenderer.h"
#include "Renderers/HktMassEntityRenderer.h"
#include "Renderers/HktUIRenderer.h"

bool UHktPresentationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

void UHktPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActorRenderer = MakeUnique<FHktActorRenderer>(GetLocalPlayer());
	MassEntityRenderer = MakeUnique<FHktMassEntityRenderer>(GetLocalPlayer());
	UIRenderer = MakeUnique<FHktUIRenderer>(GetLocalPlayer());
}

void UHktPresentationSubsystem::Deinitialize()
{
	UnbindInteraction();
	UIRenderer.Reset();
	MassEntityRenderer.Reset();
	ActorRenderer.Reset();
	State.Clear();
	Super::Deinitialize();
}

void UHktPresentationSubsystem::BindInteraction(IHktPlayerInteractionInterface* InInteraction)
{
	UnbindInteraction();
	BoundInteraction = InInteraction;
	if (BoundInteraction)
	{
		WorldViewHandle = BoundInteraction->OnWorldViewUpdated().AddUObject(
			this, &UHktPresentationSubsystem::OnWorldViewUpdated);
	}
}

void UHktPresentationSubsystem::UnbindInteraction()
{
	if (BoundInteraction && WorldViewHandle.IsValid())
	{
		BoundInteraction->OnWorldViewUpdated().Remove(WorldViewHandle);
		WorldViewHandle.Reset();
	}
	BoundInteraction = nullptr;
}

void UHktPresentationSubsystem::OnWorldViewUpdated(const FHktWorldView& View)
{
	if (!View.WorldState) return;

	if (View.bIsInitialSync || !bInitialSyncDone)
	{
		ProcessInitialSync(View);
		bInitialSyncDone = true;
	}
	else if (View.SpawnedEntities || View.RemovedEntities || View.PropertyDeltas)
	{
		ProcessDiff(View);
	}
	else
	{
		ProcessInitialSync(View);
	}
	SyncRenderers();
}

void UHktPresentationSubsystem::ProcessInitialSync(const FHktWorldView& View)
{
	State.Clear();
	State.BeginFrame(View.FrameNumber);
	View.ForEachEntity([this, &View](FHktEntityId Id, int32)
	{
		State.AddEntity(*View.WorldState, Id);
	});
}

void UHktPresentationSubsystem::ProcessDiff(const FHktWorldView& View)
{
	State.BeginFrame(View.FrameNumber);
	View.ForEachRemoved([this](FHktEntityId Id) { State.RemoveEntity(Id); });
	View.ForEachSpawned([this, &View](const FHktEntityState& ES)
	{
		State.AddEntity(*View.WorldState, ES.EntityId);
	});
	View.ForEachDelta([this](FHktEntityId Id, uint16 PropId, int32 NewValue)
	{
		State.ApplyDelta(Id, PropId, NewValue);
	});
}

void UHktPresentationSubsystem::SyncRenderers()
{
	if (ActorRenderer)      ActorRenderer->Sync(State);
	if (MassEntityRenderer) MassEntityRenderer->Sync(State);
	if (UIRenderer)         UIRenderer->Sync(State);
}
