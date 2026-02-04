// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "HktCoreTypes.h"
#include "HktModelProvider.h"
#include "HktPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UHktInputAction;
class UHktIntentBuilderComponent;
class UHktVisibleStashComponent;
class UHktVMProcessorComponent;
struct FHktFrameBatch;

/**
 * 입력을 받아 Intent를 조립하고 PlayerState로 제출하는 컨트롤러.
 */
UCLASS()
class HKTRUNTIME_API AHktPlayerController : public APlayerController, public IHktModelProvider
{
    GENERATED_BODY()

public:
    AHktPlayerController();

    // === Intent 전송 (클라이언트) ===
    
    UFUNCTION(BlueprintCallable, Category = "Hkt")
    bool SendIntent();

    // === C2S RPC ===
    
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ReceiveIntent(const FHktIntentEvent& Event);

    // === S2C RPC ===
    
    void SendBatchToOwningClient(const FHktFrameBatch& Batch);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveBatch(const FHktFrameBatch& Batch);

    // === 소유 엔티티 (클라이언트에서 계산) ===
    
    /** 내 엔티티인지 확인 (OwnerPlayerHash 또는 Owner.Self 태그) */
    UFUNCTION(BlueprintPure, Category = "Hkt|Entity")
    bool IsMyEntity(FHktEntityId EntityId) const;
    
    /** 내 소유 엔티티들 반환 */
    UFUNCTION(BlueprintPure, Category = "Hkt|Entity")
    TArray<FHktEntityId> GetMyEntities() const;
    
    /** 현재 선택된 내 엔티티 (첫 번째 소유 엔티티) */
    UFUNCTION(BlueprintPure, Category = "Hkt|Entity")
    FHktEntityId GetPrimaryEntity() const;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    //-------------------------------------------------------------------------
    // Input Handlers
    //-------------------------------------------------------------------------

    void OnSubjectAction(const FInputActionValue& Value);
    void OnTargetAction(const FInputActionValue& Value);
    void OnSlotAction(const FInputActionValue& Value, int32 SlotIndex);
    void OnZoom(const FInputActionValue& Value);

protected:
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
    
    /** Intent 빌더 컴포넌트 (클라이언트 로컬 입력 조립용) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt|Components")
    TObjectPtr<UHktIntentBuilderComponent> IntentBuilderComponent;

    // VisibleStashComponent (클라이언트 전용)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt")
    TObjectPtr<UHktVisibleStashComponent> VisibleStashComponent;
    
    /** VM 프로세서 컴포넌트 (클라이언트 로컬 시뮬레이션 실행) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hkt")
    TObjectPtr<UHktVMProcessorComponent> VMProcessorComponent;

    /** 내 PlayerHash 계산 */
    int32 GetMyPlayerHash() const;

private:
    mutable int32 CachedPlayerHash = 0;
    mutable bool bPlayerHashCached = false;

    //-------------------------------------------------------------------------
    // IHktControlProvider 델리게이트
    //-------------------------------------------------------------------------

    FOnHktSubjectChanged SubjectChangedDelegate;
    FOnHktTargetChanged TargetChangedDelegate;
    FOnHktCommandChanged CommandChangedDelegate;
    FOnHktIntentSubmitted IntentSubmittedDelegate;
    FOnHktWheelInput WheelInputDelegate;
    FOnHktEntityCreated EntityCreatedDelegate;
    FOnHktEntityDestroyed EntityDestroyedDelegate;

public:
    //-------------------------------------------------------------------------
    // IHktControlProvider 구현
    //-------------------------------------------------------------------------

    virtual IHktStashInterface* GetStashInterface() const override;
    virtual FHktEntityId GetSelectedSubject() const override;
    virtual FHktEntityId GetSelectedTarget() const override;
    virtual FVector GetTargetLocation() const override;
    virtual FGameplayTag GetSelectedCommand() const override;
    virtual bool IsIntentValid() const override;
    virtual FOnHktSubjectChanged& OnSubjectChanged() override { return SubjectChangedDelegate; }
    virtual FOnHktTargetChanged& OnTargetChanged() override { return TargetChangedDelegate; }
    virtual FOnHktCommandChanged& OnCommandChanged() override { return CommandChangedDelegate; }
    virtual FOnHktIntentSubmitted& OnIntentSubmitted() override { return IntentSubmittedDelegate; }
    virtual FOnHktWheelInput& OnWheelInput() override { return WheelInputDelegate; }
    virtual FOnHktEntityCreated& OnEntityCreated() override { return EntityCreatedDelegate; }
    virtual FOnHktEntityDestroyed& OnEntityDestroyed() override { return EntityDestroyedDelegate; }
};
