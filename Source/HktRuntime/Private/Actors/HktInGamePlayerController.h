// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "HktCoreMinimal.h"
#include "HktWorldState.h"
#include "HktRuntimeDelegates.h"
#include "HktClientRuleInterfaces.h"
#include "HktServerRuleInterfaces.h"
#include "HktRuntimeTypes.h"
#include "IHktPlayerInteractionInterface.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightProvider.h"
#endif

#include "HktIngamePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UHktInputAction;
class IHktClientRule;

UCLASS()
class HKTRUNTIME_API AHktIngamePlayerController : public APlayerController
    , public IHktPlayerInteractionInterface, public IHktInsightProvider
{
    GENERATED_BODY()

public:
    AHktIngamePlayerController();

    // === S2C RPC ===
    UFUNCTION(Client, Reliable)
    void Client_ReceiveInitialState(const FHktRuntimeSimulationState& State);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveFrameBatch(const FHktRuntimeBatch& Batch);

    // === C2S RPC ===
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ReceiveIntent(const FHktRuntimeEvent& Event);

    // === 델리게이트 ===
    FOnHktSubjectChanged& OnSubjectChanged() { return SubjectChangedDelegate; }
    FOnHktTargetChanged& OnTargetChanged() { return TargetChangedDelegate; }
    FOnHktCommandChanged& OnCommandChanged() { return CommandChangedDelegate; }
    FOnHktIntentSubmitted& OnIntentSubmitted() { return IntentSubmittedDelegate; }
    FOnHktWheelInput& OnWheelInput() { return WheelInputDelegate; }

    // === IHktPlayerInteractionInterface ===
    virtual void ExecuteCommand(UObject* CommandData) override;
    virtual bool GetWorldState(const FHktWorldState*& OutState) const override;
    virtual FOnHktWorldViewUpdated& OnWorldViewUpdated() override { return WorldViewUpdatedDelegate; }

    // === Player UID ===
    /** 인터페이스를 통해 Player UID를 반환합니다. */
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

private:
    FOnHktSubjectChanged SubjectChangedDelegate;
    FOnHktTargetChanged TargetChangedDelegate;
    FOnHktCommandChanged CommandChangedDelegate;
    FOnHktIntentSubmitted IntentSubmittedDelegate;
    FOnHktWheelInput WheelInputDelegate;
    FOnHktWorldViewUpdated WorldViewUpdatedDelegate;

    /** 클라이언트 규칙 */
    TUniquePtr<IHktClientRule> ClientRule;

    /** 캐싱된 인터페이스 포인터들 */
    IHktIntentBuilder* CachedIntentBuilder = nullptr;
    IHktUnitSelectionPolicy* CachedSelectionPolicy = nullptr;
    IHktProxySimulator* CachedProxySimulator = nullptr;
    IHktCommandContainer* CachedCommandContainer = nullptr;
    IHktWorldPlayer* CachedWorldPlayer = nullptr;

#if WITH_HKT_INSIGHTS
    /** Insight 통계: 보낸 Intent 수, 받은 배치 수 */
    int32 InsightSentIntentCount = 0;
    int32 InsightReceivedBatchCount = 0;
    int32 InsightReceivedInitialStateCount = 0;
#endif
};
