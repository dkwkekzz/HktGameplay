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
#include "Renderers/HktVFXRenderer.h"
#include "HktPresentationRenderer.h"
#include "HktPresentationSubsystem.generated.h"

class IHktPlayerInteractionInterface;
class FHktActorRenderer;
class FHktMassEntityRenderer;
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

	/** 외부 렌더러 등록/해제 (예: AHktIngameHUD). 등록 시 기존 State 즉시 Sync. */
	void RegisterRenderer(IHktPresentationRenderer* InRenderer);
	void UnregisterRenderer(IHktPresentationRenderer* InRenderer);

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

	/** State 변경 시 전체 Sync, 아니면 NeedsTick/NeedsCameraSync인 렌더러만 Sync */
	void OnTick(float DeltaSeconds);

	/** 카메라 뷰 변경 감지 (위치/회전이 달라지면 true) */
	bool DetectCameraChange();

	FDelegateHandle TickHandle;
	FHktPresentationState State;

	/** IHktPresentationRenderer::Sync 루프에 참여하는 모든 렌더러 */
	TArray<IHktPresentationRenderer*> Renderers;

	/** 렌더러별 전용 API 접근용 (GetActor, PlayVFX 등) */
	TUniquePtr<FHktActorRenderer> ActorRenderer;
	TUniquePtr<FHktMassEntityRenderer> MassEntityRenderer;
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

	/** 카메라 뷰 변경 감지용 캐시 */
	FVector CachedCameraLocation = FVector::ZeroVector;
	FRotator CachedCameraRotation = FRotator::ZeroRotator;
};
