// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "HktPresentationTypes.h"
#include "HktPresentationSubsystem.generated.h"

// Forward declarations
class IHktModelProvider;
class IHktStashInterface;
class AHktCharacter;
class AHktRtsCameraPawn;
class APlayerController;

class FHktEntityVisualManager;
class FHktSelectionVisualManager;
class FHktInteractionFXManager;
class FHktEntityHUDManager;

// ============================================================================
// Presentation 이벤트 (ModelProvider 구독 후 Subsystem이 브로드캐스트, CameraPawn 등이 구독)
// ============================================================================

DECLARE_MULTICAST_DELEGATE_OneParam(FHktPresentationWheelInput, float);

/**
 * UHktPresentationSubsystem
 * 
 * HktPresentation의 메인 LocalPlayerSubsystem
 * 각 시각화 책임을 Manager 클래스들에 위임
 * 
 * Manager 구성:
 * - FHktEntityVisualManager: 엔티티 Actor Spawn/Destroy
 * - FHktSelectionVisualManager: 선택 데칼 표시
 * - FHktInteractionFXManager: 인터랙션 이펙트
 * - FHktEntityHUDManager: 체력바/ID HUD
 */
UCLASS()
class HKTPRESENTATION_API UHktPresentationSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UHktPresentationSubsystem();
	~UHktPresentationSubsystem();

	static UHktPresentationSubsystem* Get(APlayerController* PC);

	// === USubsystem Interface ===
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void BeginDestroy() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// === ULocalPlayerSubsystem ===
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	// === FTickableGameObject Interface ===
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// === Model Provider 바인딩 ===
	
	UFUNCTION(BlueprintCallable, Category = "Hkt|Presentation")
	void BindModelProvider(TScriptInterface<IHktModelProvider> InProvider);

	UFUNCTION(BlueprintCallable, Category = "Hkt|Presentation")
	void UnbindModelProvider();

	UFUNCTION(BlueprintPure, Category = "Hkt|Presentation")
	TScriptInterface<IHktModelProvider> GetModelProvider() const { return ModelProvider; }

	/** ModelProvider OnWheelInput 수신 시 Subsystem이 브로드캐스트. HktRtsCameraPawn 등이 구독 */
	FHktPresentationWheelInput OnWheelInput;

	// === Manager 접근 ===
	
	FHktEntityVisualManager* GetEntityVisualManager() const { return EntityVisualManager; }
	FHktSelectionVisualManager* GetSelectionVisualManager() const { return SelectionVisualManager; }
	FHktInteractionFXManager* GetInteractionFXManager() const { return InteractionFXManager; }
	FHktEntityHUDManager* GetEntityHUDManager() const { return EntityHUDManager; }

	// === 엔티티 조회 (EntityVisualManager 위임) ===
	
	UFUNCTION(BlueprintPure, Category = "Hkt|Presentation")
	AActor* GetEntityActor(FHktEntityId EntityId) const;

	UFUNCTION(BlueprintPure, Category = "Hkt|Presentation")
	TArray<AActor*> GetAllEntityActors() const;

protected:
	// === 이벤트 핸들러 ===
	void HandleSubjectChanged(FHktEntityId EntityId);
	void HandleTargetChanged(FHktEntityId EntityId);
	void HandleCommandChanged(FGameplayTag Command);
	void HandleIntentSubmitted(const FHktIntentEvent& Event);
	void HandleWheelInput(float Delta);
	void HandleEntityCreated(FHktEntityId EntityId);
	void HandleEntityDestroyed(FHktEntityId EntityId);

	void CreateManagers();
	void DestroyManagers();
	void SyncEntitiesFromStash();

private:
	// === Model Provider ===
	UPROPERTY()
	TScriptInterface<IHktModelProvider> ModelProvider;

	// === Managers (설계대로 분리) ===
	FHktEntityVisualManager* EntityVisualManager;
	FHktSelectionVisualManager* SelectionVisualManager;
	FHktInteractionFXManager* InteractionFXManager;
	FHktEntityHUDManager* EntityHUDManager;

	// === 델리게이트 핸들 ===
	FDelegateHandle SubjectChangedHandle;
	FDelegateHandle TargetChangedHandle;
	FDelegateHandle CommandChangedHandle;
	FDelegateHandle IntentSubmittedHandle;
	FDelegateHandle WheelInputHandle;

	bool bIsBound = false;
	bool bInitialized = false;
};
