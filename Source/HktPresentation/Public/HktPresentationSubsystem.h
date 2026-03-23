// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "HktCoreDefs.h"
#include "HktWorldView.h"
#include "HktPresentationState.h"
// TODO: 전방선언 이슈
#include "Renderers/HktActorRenderer.h"
#include "Renderers/HktMassEntityRenderer.h"
#include "Renderers/HktUIRenderer.h"
#include "Renderers/HktVFXRenderer.h"
#include "HktPresentationSubsystem.generated.h"

class IHktPlayerInteractionInterface;
class FHktActorRenderer;
class FHktMassEntityRenderer;
class FHktUIRenderer;
class FHktVFXRenderer;
struct FHktRuntimeEvent;
struct FHktVFXIntent;

/** WorldState → PresentationState → Renderer 파이프라인. LocalPlayer당 1개. */
UCLASS()
class HKTPRESENTATION_API UHktPresentationSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UHktPresentationSubsystem* Get(APlayerController* PC);
	
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// === ULocalPlayerSubsystem ===
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	const FHktPresentationState& GetState() const { return State; }

	/** 엔티티에 해당하는 렌더링 Actor를 반환. 없으면 nullptr. */
	AActor* GetRenderedActor(FHktEntityId Id) const;

	/** 월드 위치에 VFX 재생 (클라이언트 즉시, 서버 무관) */
	void PlayVFXAtLocation(FGameplayTag VFXTag, FVector Location);

	/** Intent 기반 VFX 재생 (AssetBank 퍼지 매칭 + RuntimeOverrides) */
	void PlayVFXWithIntent(const FHktVFXIntent& Intent);

private:
	/** PlayerController 바인딩 (BeginPlay 등에서 호출) */
	void BindInteraction(IHktPlayerInteractionInterface* InInteraction);
	void UnbindInteraction();

	void OnWorldViewUpdated(const FHktWorldView& View);
	void OnIntentSubmitted(const FHktRuntimeEvent& Event);
	void OnSubjectChanged(FHktEntityId NewSubject);
	void OnTargetChanged(FHktEntityId NewTarget);
	void ProcessInitialSync(const FHktWorldView& View);
	void ProcessDiff(const FHktWorldView& View);
	void SyncRenderers();

	/** State 변경 시 전체 Sync, 아니면 NeedsTick인 렌더러만 Sync */
	void OnTick(float DeltaSeconds);

	FDelegateHandle TickHandle;
	FHktPresentationState State;
	TUniquePtr<FHktActorRenderer> ActorRenderer;
	TUniquePtr<FHktMassEntityRenderer> MassEntityRenderer;
	TUniquePtr<FHktUIRenderer> UIRenderer;
	TUniquePtr<FHktVFXRenderer> VFXRenderer;

	IHktPlayerInteractionInterface* BoundInteraction = nullptr;
	FDelegateHandle WorldViewHandle;
	FDelegateHandle IntentSubmittedHandle;
	FDelegateHandle SubjectChangedHandle;
	FDelegateHandle TargetChangedHandle;

	/** 현재 선택된 Subject/Target 엔터티 (VFX 추적용) */
	FHktEntityId CurrentSubjectEntityId = InvalidEntityId;
	FHktEntityId CurrentTargetEntityId = InvalidEntityId;

	bool bInitialSyncDone = false;
	bool bStateDirty = false;
};
