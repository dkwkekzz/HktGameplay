// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPresentationSubsystem.h"
#include "IHktPlayerInteractionInterface.h"
#include "HktRuntimeTypes.h"
#include "Renderers/HktActorRenderer.h"
#include "Renderers/HktMassEntityRenderer.h"
#include "Renderers/HktVFXRenderer.h"
#include "NativeGameplayTags.h"
#include "HktPresentationLog.h"
#include "HktCoreEventLog.h"
#include "HktRuntimeTags.h"
#include "HktAssetSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"


UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_VFX_MoveIndicator, "VFX.Niagara.MoveIndicator");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_VFX_SelectionSubject, "VFX.Niagara.SelectionSubject");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_VFX_SelectionTarget, "VFX.Niagara.SelectionTarget");
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

	ActorRenderer = MakeShared<FHktActorRenderer>(GetLocalPlayer());
	MassEntityRenderer = MakeShared<FHktMassEntityRenderer>(GetLocalPlayer());
	VFXRenderer = MakeShared<FHktVFXRenderer>(GetLocalPlayer());

	// 내부 렌더러를 Sync 루프에 등록
	Renderers.Add(ActorRenderer.Get());
	Renderers.Add(MassEntityRenderer.Get());
	Renderers.Add(VFXRenderer.Get());
}

void UHktPresentationSubsystem::Deinitialize()
{
	UnbindInteraction();

	for (IHktPresentationRenderer* R : Renderers)
	{
		if (R) R->Teardown();
	}
	Renderers.Empty();

	VFXRenderer.Reset();
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
		SubjectChangedHandle = BoundInteraction->OnSubjectChanged().AddUObject(
			this, &UHktPresentationSubsystem::OnSubjectChanged);
		TargetChangedHandle = BoundInteraction->OnTargetChanged().AddUObject(
			this, &UHktPresentationSubsystem::OnTargetChanged);

		// 렌더러 pending 작업 처리용 틱 등록
		if (!TickHandle.IsValid())
		{
			if (UWorld* World = GetLocalPlayer()->GetWorld())
			{
				TickHandle = World->OnTickDispatch().AddUObject(
					this, &UHktPresentationSubsystem::OnTick);
			}
		}

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
		if (SubjectChangedHandle.IsValid())
		{
			BoundInteraction->OnSubjectChanged().Remove(SubjectChangedHandle);
			SubjectChangedHandle.Reset();
		}
		if (TargetChangedHandle.IsValid())
		{
			BoundInteraction->OnTargetChanged().Remove(TargetChangedHandle);
			TargetChangedHandle.Reset();
		}
	}
	if (TickHandle.IsValid())
	{
		if (ULocalPlayer* LP = GetLocalPlayer())
		{
			if (UWorld* World = LP->GetWorld())
			{
				World->OnTickDispatch().Remove(TickHandle);
			}
		}
		TickHandle.Reset();
	}
	BoundInteraction = nullptr;
}

void UHktPresentationSubsystem::OnWorldViewUpdated(const FHktWorldView& View)
{
	if (!View.WorldState) return;

	if (View.bIsInitialSync || !bInitialSyncDone)
	{
		HKT_EVENT_LOG(HktLogTags::Presentation,
			FString::Printf(TEXT("InitialSync Frame=%lld Entities=%d"),
				View.FrameNumber, View.WorldState->GetEntityCount()));
		ProcessInitialSync(View);
		bInitialSyncDone = true;
		bStateDirty = true;
	}
	else if (View.SpawnedEntities || View.RemovedEntities || View.PropertyDeltas || View.TagDeltas || View.OwnerDeltas)
	{
		ProcessDiff(View);
		bStateDirty = true;
	}
}

void UHktPresentationSubsystem::ProcessInitialSync(const FHktWorldView& View)
{
	State.Clear();
	State.BeginFrame(View.FrameNumber);
	View.ForEachEntity([this, &View](FHktEntityId Id, int32)
	{
		State.AddEntity(*View.WorldState, Id);
	});
	ResolveAssetPathsForSpawned();
}

void UHktPresentationSubsystem::ProcessDiff(const FHktWorldView& View)
{
	State.BeginFrame(View.FrameNumber);

	int32 RemovedCount = 0;
	View.ForEachRemoved([this, &RemovedCount](FHktEntityId Id) { State.RemoveEntity(Id); ++RemovedCount; });
	int32 SpawnedCount = 0;
	View.ForEachSpawned([this, &View, &SpawnedCount](const FHktEntityState& ES)
	{
		State.AddEntity(*View.WorldState, ES.EntityId);
		++SpawnedCount;
	});

	if (SpawnedCount > 0 || RemovedCount > 0)
	{
		HKT_EVENT_LOG(HktLogTags::Presentation, FString::Printf(TEXT("ProcessDiff Frame=%lld Spawned=%d Removed=%d"), View.FrameNumber, SpawnedCount, RemovedCount));
	}
	ResolveAssetPathsForSpawned();

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

void UHktPresentationSubsystem::OnTick(float DeltaSeconds)
{
	if (!bInitialSyncDone) return;

	if (bStateDirty)
	{
		bStateDirty = false;
		SyncRenderers();
	}
	else
	{
		for (IHktPresentationRenderer* R : Renderers)
		{
			if (R && R->NeedsTick())
			{
				R->Sync(State);
			}
		}
	}
}

void UHktPresentationSubsystem::NotifyCameraViewChanged()
{
	if (!bInitialSyncDone) return;

	for (IHktPresentationRenderer* R : Renderers)
	{
		if (R && R->NeedsCameraSync())
		{
			R->Sync(State);
		}
	}
}

void UHktPresentationSubsystem::ResolveAssetPathsForSpawned()
{
	UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
	UHktAssetSubsystem* AssetSubsystem = World ? UHktAssetSubsystem::Get(World) : nullptr;
	if (!AssetSubsystem) return;

	for (FHktEntityId Id : State.SpawnedThisFrame)
	{
		FHktEntityPresentation* E = State.GetMutable(Id);
		if (!E) continue;
		FGameplayTag VisualTag = E->VisualElement.Get();
		if (!VisualTag.IsValid()) continue;

		// 비동기 로드 → 완료 시 ViewModel에 ResolvedAssetPath 설정
		AssetSubsystem->LoadAssetAsync(VisualTag, [this, Id](UHktTagDataAsset* Asset)
		{
			if (!Asset) return;
			FHktEntityPresentation* E = State.GetMutable(Id);
			if (!E || !E->IsAlive()) return;
			E->ResolvedAssetPath.Set(FSoftObjectPath(Asset), State.GetCurrentFrame());
		});
	}
}

void UHktPresentationSubsystem::SyncRenderers()
{
	for (IHktPresentationRenderer* R : Renderers)
	{
		if (R) R->Sync(State);
	}
}

FVector UHktPresentationSubsystem::GetEntityLocation(FHktEntityId Id) const
{
	if (ActorRenderer)
	{
		if (const AActor* Actor = ActorRenderer->GetActor(Id))
		{
			return Actor->GetActorLocation();
		}
	}

	const FHktEntityPresentation* E = State.Get(Id);
	return E ? E->Location.Get() : FVector::ZeroVector;
}

void UHktPresentationSubsystem::RegisterRenderer(IHktPresentationRenderer* InRenderer)
{
	if (!InRenderer || Renderers.Contains(InRenderer)) return;

	Renderers.Add(InRenderer);

	// 이미 InitialSync가 완료된 경우 즉시 OnRegistered 호출하여 기존 엔티티 전달
	if (bInitialSyncDone)
	{
		InRenderer->OnRegistered(State);
	}
}

void UHktPresentationSubsystem::UnregisterRenderer(IHktPresentationRenderer* InRenderer)
{
	Renderers.Remove(InRenderer);
}

void UHktPresentationSubsystem::OnIntentSubmitted(const FHktRuntimeEvent& Event)
{
	const FHktEvent& CoreEvent = Event.Value;

	// MoveTo intent → 목표 위치에 이동 인디케이터 VFX 재생
	if (CoreEvent.EventTag.MatchesTag(HktGameplayTags::Story_Event_Move_ToLocation))
	{
		PlayVFXAtLocation(Tag_VFX_MoveIndicator, CoreEvent.Location);
	}
}

void UHktPresentationSubsystem::PlayVFXAtLocation(FGameplayTag VFXTag, FVector Location)
{
	HKT_EVENT_LOG(HktLogTags::Presentation, FString::Printf(TEXT("PlayVFXAtLocation Tag=%s Location=(%.1f, %.1f, %.1f)"), *VFXTag.ToString(), Location.X, Location.Y, Location.Z));

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

void UHktPresentationSubsystem::OnSubjectChanged(FHktEntityId NewSubject)
{
	if (!VFXRenderer) return;

	// 이전 Subject VFX 제거
	if (CurrentSubjectEntityId != InvalidEntityId)
	{
		VFXRenderer->DetachVFXFromEntity(Tag_VFX_SelectionSubject, CurrentSubjectEntityId);
	}

	CurrentSubjectEntityId = NewSubject;

	// 새 Subject VFX 부착
	if (NewSubject != InvalidEntityId)
	{
		const FHktEntityPresentation* Entity = State.Get(NewSubject);
		FVector Pos = Entity ? Entity->Location.Get() : FVector::ZeroVector;
		VFXRenderer->AttachVFXToEntity(Tag_VFX_SelectionSubject, NewSubject, Pos);

		HKT_EVENT_LOG(HktLogTags::Presentation, FString::Printf(TEXT("SelectionSubject VFX attached Entity=%d"), NewSubject));
	}
}

void UHktPresentationSubsystem::OnTargetChanged(FHktEntityId NewTarget)
{
	if (!VFXRenderer) return;

	// 이전 Target VFX 제거
	if (CurrentTargetEntityId != InvalidEntityId)
	{
		VFXRenderer->DetachVFXFromEntity(Tag_VFX_SelectionTarget, CurrentTargetEntityId);
	}

	CurrentTargetEntityId = NewTarget;

	// 새 Target VFX 부착
	if (NewTarget != InvalidEntityId)
	{
		const FHktEntityPresentation* Entity = State.Get(NewTarget);
		FVector Pos = Entity ? Entity->Location.Get() : FVector::ZeroVector;
		VFXRenderer->AttachVFXToEntity(Tag_VFX_SelectionTarget, NewTarget, Pos);

		HKT_EVENT_LOG(HktLogTags::Presentation, FString::Printf(TEXT("SelectionTarget VFX attached Entity=%d"), NewTarget));
	}
}
