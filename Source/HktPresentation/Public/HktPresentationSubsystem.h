// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "HktCoreMinimal.h"
#include "HktWorldView.h"
#include "HktPresentationState.h"
// TODO: 전방선언 이슈
#include "Renderers/HktActorRenderer.h"
#include "Renderers/HktMassEntityRenderer.h"
#include "Renderers/HktUIRenderer.h"
#include "HktPresentationSubsystem.generated.h"

class IHktPlayerInteractionInterface;
class FHktActorRenderer;
class FHktMassEntityRenderer;
class FHktUIRenderer;

/** WorldState → PresentationState → Renderer 파이프라인. LocalPlayer당 1개. */
UCLASS()
class HKTPRESENTATION_API UHktPresentationSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** PlayerController 바인딩 (BeginPlay 등에서 호출) */
	void BindInteraction(IHktPlayerInteractionInterface* InInteraction);
	void UnbindInteraction();

	const FHktPresentationState& GetState() const { return State; }

private:
	void OnWorldViewUpdated(const FHktWorldView& View);
	void ProcessInitialSync(const FHktWorldView& View);
	void ProcessDiff(const FHktWorldView& View);
	void SyncRenderers();

	FHktPresentationState State;
	TUniquePtr<FHktActorRenderer> ActorRenderer;
	TUniquePtr<FHktMassEntityRenderer> MassEntityRenderer;
	TUniquePtr<FHktUIRenderer> UIRenderer;

	IHktPlayerInteractionInterface* BoundInteraction = nullptr;
	FDelegateHandle WorldViewHandle;
	bool bInitialSyncDone = false;
};
