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
class UHktVisibleStashComponent;
class UHktVMProcessorComponent;
class UHktCommandContainerComponent;
class IHktClientRule;
struct FHktFrameBatch;
struct FHktGroupSimulationState;

/**
 * AHktInGamePlayerController - 클라이언트 오케스트레이터
 *
 * 아키텍처 원칙:
 *   - Actor는 "이벤트 발행"에 집중 (인터페이스를 직접 구현하지 않음)
 *   - 입력 이벤트 → ClientRule에 위임
 *   - 서버 수신 이벤트 → ClientRule에 위임
 *   - Component가 인터페이스 구현을 담당
 *
 * 이벤트 → Rule 매핑:
 *   OnSubjectAction  → Rule->OnUserEvent_SubjectInputAction()
 *   OnTargetAction   → Rule->OnUserEvent_TargetInputAction()
 *   OnSlotAction     → Rule->OnUserEvent_CommandInputAction()
 *   OnZoom           → Rule->OnUserEvent_ZoomInputAction()
 *   Client_ReceiveFrameBatch        → Rule->OnReceived_FrameBatch()
 *   Client_ReceiveInitialState      → Rule->OnReceived_InitialSimulationState()
 */
UCLASS()
class HKTRUNTIME_API AHktInGamePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AHktInGamePlayerController();

    // === S2C RPC (서버 → 클라이언트) ===

    UFUNCTION(Client, Reliable)
    void Client_ReceiveFrameBatch(const FHktFrameBatch& Batch);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveInitialState(const FHktGroupSimulationState& State);

    // === C2S RPC (클라이언트 → 서버) ===

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ReceiveIntent(const FHktIntentEvent& Event);

    // === 델리게이트 (Presentation 레이어 연결용) ===

    FOnHktSubjectChanged& OnSubjectChanged() { return SubjectChangedDelegate; }
    FOnHktTargetChanged& OnTargetChanged() { return TargetChangedDelegate; }
    FOnHktCommandChanged& OnCommandChanged() { return CommandChangedDelegate; }
    FOnHktIntentSubmitted& OnIntentSubmitted() { return IntentSubmittedDelegate; }
    FOnHktWheelInput& OnWheelInput() { return WheelInputDelegate; }
    FOnHktEntityCreated& OnEntityCreated() { return EntityCreatedDelegate; }
    FOnHktEntityDestroyed& OnEntityDestroyed() { return EntityDestroyedDelegate; }

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // === 입력 핸들러 (이벤트 발행 → Rule 위임) ===

    void OnSubjectAction(const FInputActionValue& Value);
    void OnTargetAction(const FInputActionValue& Value);
    void OnSlotAction(const FInputActionValue& Value, int32 SlotIndex);
    void OnZoom(const FInputActionValue& Value);

    // === Rule 조회 ===
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

    // === 인터페이스 구현 컴포넌트들 (Rule의 파라미터로 전달됨) ===

    /** IHktIntentBuilder 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktIntentBuilderComponent> IntentBuilderComponent;

    /** VisibleStash (클라이언트 전용) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktVisibleStashComponent> VisibleStashComponent;

    /** VM 프로세서 (클라이언트 로컬 시뮬레이션) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktVMProcessorComponent> VMProcessorComponent;

    /** IHktCommandContainer 구현 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktCommandContainerComponent> CommandContainerComponent;

private:
    // 델리게이트
    FOnHktSubjectChanged SubjectChangedDelegate;
    FOnHktTargetChanged TargetChangedDelegate;
    FOnHktCommandChanged CommandChangedDelegate;
    FOnHktIntentSubmitted IntentSubmittedDelegate;
    FOnHktWheelInput WheelInputDelegate;
    FOnHktEntityCreated EntityCreatedDelegate;
    FOnHktEntityDestroyed EntityDestroyedDelegate;
};
