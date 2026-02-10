// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "HktCoreTypes.h"
#include "HktRuntimeDelegates.h"
#include "HktInGamePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UHktInputAction;
class UHktIntentBuilderComponent;
class UHktClientSimulatorComponent;
class UHktCommandContainerComponent;
class UHktWorldPlayerComponent;
class IHktClientRule;
struct FHktFrameBatch;
struct FHktGroupSimulationState;

UCLASS()
class HKTRUNTIME_API AHktInGamePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHktInGamePlayerController();

    // === S2C RPC ===
    UFUNCTION(Client, Reliable)
    void Client_ReceiveFrameBatch(const FHktFrameBatch& Batch);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveInitialState(const FHktGroupSimulationState& State);

    // === C2S RPC ===
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ReceiveIntent(const FHktIntentEvent& Event);

    // === 델리게이트 ===
    FOnHktSubjectChanged& OnSubjectChanged() { return SubjectChangedDelegate; }
    FOnHktTargetChanged& OnTargetChanged() { return TargetChangedDelegate; }
    FOnHktCommandChanged& OnCommandChanged() { return CommandChangedDelegate; }
    FOnHktIntentSubmitted& OnIntentSubmitted() { return IntentSubmittedDelegate; }
    FOnHktWheelInput& OnWheelInput() { return WheelInputDelegate; }

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    void OnSubjectAction(const FInputActionValue& Value);
    void OnTargetAction(const FInputActionValue& Value);
    void OnSlotAction(const FInputActionValue& Value, int32 SlotIndex);
    void OnZoom(const FInputActionValue& Value);

    IHktClientRule* GetClientRule() const;

protected:
    // === Input ===
    UPROPERTY(EditDefaultsOnly, Category = "Hkt|Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Hkt|Input")
    TObjectPtr<UInputAction> SubjectAction;

    UPROPERTY(EditDefaultsOnly, Category = "Hkt|Input")
    TObjectPtr<UInputAction> TargetAction;

    UPROPERTY(EditDefaultsOnly, Category = "Hkt|Input")
    TObjectPtr<UInputAction> ZoomAction;

    UPROPERTY(EditDefaultsOnly, Category = "Hkt|Input")
    TArray<TObjectPtr<UHktInputAction>> SlotActions;

    // === 컴포넌트 ===

    /** IHktIntentBuilder + IHktSubjectSelectionPolicy + IHktTargetSelectionPolicy (클라이언트) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktIntentBuilderComponent> IntentBuilderComponent;

    /** IHktSimulator (클라이언트) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktClientSimulatorComponent> ClientSimulatorComponent;

    /** IHktCommandContainer (클라이언트) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktCommandContainerComponent> CommandContainerComponent;

    /** IHktWorldPlayer (서버) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktWorldPlayerComponent> WorldPlayerComponent;

private:
    FOnHktSubjectChanged SubjectChangedDelegate;
    FOnHktTargetChanged TargetChangedDelegate;
    FOnHktCommandChanged CommandChangedDelegate;
    FOnHktIntentSubmitted IntentSubmittedDelegate;
    FOnHktWheelInput WheelInputDelegate;
};
