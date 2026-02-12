// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "HktCoreTypes.h"
#include "HktRuntimeDelegates.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightProvider.h"
#endif

#include "HktInGamePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UHktInputAction;
class UHktIntentBuilderComponent;
class UHktDesktopDefaultSelectionPolicy;
class UHktClientSimulatorComponent;
class UHktCommandContainerComponent;
class UHktWorldPlayerComponent;
class IHktClientRule;
struct FHktRuntimeBatch;
struct FHktRuntimeSimulationState;

UCLASS()
class HKTRUNTIME_API AHktInGamePlayerController : public APlayerController
#if WITH_HKT_INSIGHTS
    , public IHktInsightProvider
#endif
{
    GENERATED_BODY()

public:
    AHktInGamePlayerController();

    // === S2C RPC ===
    UFUNCTION(Client, Reliable)
    void Client_ReceiveFrameBatch(const FHktRuntimeBatch& Batch);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveInitialState(const FHktRuntimeSimulationState& State);

    // === C2S RPC ===
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ReceiveIntent(const FHktRuntimeEvent& Event);

    // === 델리게이트 ===
    FOnHktSubjectChanged& OnSubjectChanged() { return SubjectChangedDelegate; }
    FOnHktTargetChanged& OnTargetChanged() { return TargetChangedDelegate; }
    FOnHktCommandChanged& OnCommandChanged() { return CommandChangedDelegate; }
    FOnHktIntentSubmitted& OnIntentSubmitted() { return IntentSubmittedDelegate; }
    FOnHktWheelInput& OnWheelInput() { return WheelInputDelegate; }

    // === Player UID ===
    /** PlayerState의 UniqueId로부터 계산된 UID를 반환합니다. 한 번 계산되면 캐시됩니다. */
    int64 GetPlayerUid() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void OnRep_PlayerState() override;

    void OnSubjectAction(const FInputActionValue& Value);
    void OnTargetAction(const FInputActionValue& Value);
    void OnSlotAction(const FInputActionValue& Value, int32 SlotIndex);
    void OnZoom(const FInputActionValue& Value);

    IHktClientRule* GetClientRule() const;

#if WITH_HKT_INSIGHTS
public:
    virtual void CollectInsightData(FHktInsightSnapshot& OutSnapshot) const override;
    virtual FString GetInsightProviderName() const override { return TEXT("PlayerController"); }
#endif

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

    /** IHktIntentBuilder (클라이언트) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktIntentBuilderComponent> IntentBuilderComponent;

    /** IHktUnitSelectionPolicy (클라이언트) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktDesktopDefaultSelectionPolicy> SelectionPolicyComponent;

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

    // Player UID 캐시
    mutable int64 CachedPlayerUid = 0;
    mutable bool bPlayerUidCached = false;

#if WITH_HKT_INSIGHTS
    /** Insight 통계: 보낸 Intent 수, 받은 배치 수 */
    int32 InsightSentIntentCount = 0;
    int32 InsightReceivedBatchCount = 0;
    int32 InsightReceivedInitialStateCount = 0;
#endif
};
