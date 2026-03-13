// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPresentationSubsystem.h"
#include "IHktPlayerInteractionInterface.h"
#include "HktRuntimeTypes.h"
#include "Renderers/HktActorRenderer.h"
#include "Renderers/HktMassEntityRenderer.h"
#include "Renderers/HktUIRenderer.h"
#include "Renderers/HktVFXRenderer.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Action_Move_ToLocation, "Action.Move.ToLocation");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_VFX_MoveIndicator, "VFX.MoveIndicator");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_VFX_Prefix, "VFX");

UHktPresentationSubsystem* UHktPresentationSubsystem::Get(APlayerController* PC)
{
	if (PC && PC->GetLocalPlayer())
	{
		return PC->GetLocalPlayer()->GetSubsystem<UHktPresentationSubsystem>();
	}
	return nullptr;
}

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
	VFXRenderer = MakeUnique<FHktVFXRenderer>(GetLocalPlayer());
}

void UHktPresentationSubsystem::Deinitialize()
{
	UnbindInteraction();

	if (VFXRenderer) VFXRenderer->Teardown();
	VFXRenderer.Reset();
	UIRenderer.Reset();
	MassEntityRenderer.Reset();
	ActorRenderer.Reset();
	State.Clear();
	
	Super::Deinitialize();
}

void UHktPresentationSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	
	if (NewPlayerController)
	{
		IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(NewPlayerController);
		if (Interaction)
		{
			BindInteraction(Interaction);
		}
	}
	else
	{
		UnbindInteraction();
	}
}

void UHktPresentationSubsystem::BindInteraction(IHktPlayerInteractionInterface* InInteraction)
{
	UnbindInteraction();
	BoundInteraction = InInteraction;
	if (BoundInteraction)
	{
		WorldViewHandle = BoundInteraction->OnWorldViewUpdated().AddUObject(
			this, &UHktPresentationSubsystem::OnWorldViewUpdated);
		IntentSubmittedHandle = BoundInteraction->OnIntentSubmitted().AddUObject(
			this, &UHktPresentationSubsystem::OnIntentSubmitted);
	}
}

void UHktPresentationSubsystem::UnbindInteraction()
{
	if (BoundInteraction)
	{
		if (WorldViewHandle.IsValid())
		{
			BoundInteraction->OnWorldViewUpdated().Remove(WorldViewHandle);
			WorldViewHandle.Reset();
		}
		if (IntentSubmittedHandle.IsValid())
		{
			BoundInteraction->OnIntentSubmitted().Remove(IntentSubmittedHandle);
			IntentSubmittedHandle.Reset();
		}
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
	else if (View.SpawnedEntities || View.RemovedEntities || View.PropertyDeltas || View.TagDeltas || View.OwnerDeltas)
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
	View.ForEachOwnerDelta([this](FHktEntityId Id, int64 NewOwnerUid)
	{
		State.ApplyOwnerDelta(Id, NewOwnerUid);
	});

	// 태그 델타 처리
	View.ForEachTagDelta([this, &View](FHktEntityId Id, const FGameplayTagContainer& Tags, const FGameplayTagContainer& OldTags)
	{
		// Entity presentation에 태그 동기화 (AnimInstance 태그 기반 애니메이션용)
		State.ApplyTagDelta(Id, Tags);

		// VFX 태그 감지: OldTags에 없고 Tags에 있는 "VFX." 접두사 태그 → VFXRenderer 호출
		FGameplayTagContainer NewlyAdded = Tags.Filter(FGameplayTagContainer(Tag_VFX_Prefix));
		FGameplayTagContainer OldVFX = OldTags.Filter(FGameplayTagContainer(Tag_VFX_Prefix));

		for (const FGameplayTag& Tag : NewlyAdded)
		{
			if (!OldVFX.HasTag(Tag) && VFXRenderer && View.WorldState)
			{
				FIntVector IntPos = View.WorldState->GetPosition(Id);
				FVector Pos(IntPos.X, IntPos.Y, IntPos.Z);
				VFXRenderer->PlayVFXAtLocation(Tag, Pos);
			}
		}
	});
}

void UHktPresentationSubsystem::SyncRenderers()
{
	if (ActorRenderer)      ActorRenderer->Sync(State);
	if (MassEntityRenderer) MassEntityRenderer->Sync(State);
	if (UIRenderer)         UIRenderer->Sync(State);
}

void UHktPresentationSubsystem::OnIntentSubmitted(const FHktRuntimeEvent& Event)
{
	const FHktEvent& CoreEvent = Event.Value;

	// MoveTo intent → 목표 위치에 이동 인디케이터 VFX 재생
	if (CoreEvent.EventTag.MatchesTag(Tag_Action_Move_ToLocation))
	{
		PlayVFXAtLocation(Tag_VFX_MoveIndicator, CoreEvent.Location);
	}
}

void UHktPresentationSubsystem::PlayVFXAtLocation(FGameplayTag VFXTag, FVector Location)
{
	if (VFXRenderer)
	{
		VFXRenderer->PlayVFXAtLocation(VFXTag, Location);
	}
}

void UHktPresentationSubsystem::PlayVFXWithIntent(const FHktVFXIntent& Intent)
{
	if (VFXRenderer)
	{
		VFXRenderer->PlayVFXWithIntent(Intent);
	}
}
