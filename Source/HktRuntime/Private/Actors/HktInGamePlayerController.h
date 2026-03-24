// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "HktCoreDefs.h"
#include "HktWorldState.h"
#include "HktRuntimeDelegates.h"
#include "HktClientRuleInterfaces.h"
#include "HktServerRuleInterfaces.h"
#include "HktRuntimeTypes.h"
#include "IHktPlayerInteractionInterface.h"

#include "HktIngamePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class IHktClientRule;
class UHktBagComponent;
struct FHktWorldView;

UCLASS()
class HKTRUNTIME_API AHktIngamePlayerController : public APlayerController
    , public IHktPlayerInteractionInterface
{
    GENERATED_BODY()

public:
    AHktIngamePlayerController();

    // === S2C RPC ===
    UFUNCTION(Client, Reliable)
    void Client_ReceiveInitialState(const FHktRuntimeSimulationState& State, int32 GroupIndex);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveFrameBatch(const FHktRuntimeBatch& Batch);

    // === C2S RPC (통일) ===

    /** Subject+Target 상호작용 통합 요청 (이동, 공격, 픽업, 스킬 등) */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ReceiveRuntimeEvent(const FHktRuntimeEvent& Event);

    /** UI 직접 아이템 조작 (activate, deactivate, drop — 인벤토리 패널 전용) */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ReceiveItemRequest(const FHktRuntimeItemRequest& Request);

    // === C2S Bag RPC ===
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ReceiveBagRequest(const FHktRuntimeBagRequest& Request);

    // === 아이템 상호작용 (UI에서 호출) ===

    /** 인벤토리 아이템 활성화 (Equipment 슬롯으로 이동) */
    void RequestItemActivate(FHktEntityId ItemEntity, int32 ActionSlot);

    /** 장비 아이템 비활성화 (인벤토리로 이동) */
    void RequestItemDeactivate(FHktEntityId ItemEntity);

    /** 아이템 드롭 (바닥에 놓기) */
    void RequestItemDrop(FHktEntityId ItemEntity);

    // === 가방 상호작용 (UI/입력에서 호출) ===

    /** ItemSlot → Bag (장비 슬롯에서 가방으로 보관) */
    void RequestBagStore(int32 ActionSlot);

    /** Bag → ItemSlot (가방에서 장비 슬롯으로 장착) */
    void RequestBagRestore(int32 BagSlot, int32 ActionSlot);

    /** Bag → Ground (가방에서 바닥으로 버리기) */
    void RequestBagDiscard(int32 BagSlot);

    /** 가방 컴포넌트 접근 */
    UHktBagComponent* GetBagComponent() const { return CachedBagComponent; }

    // === 델리게이트 ===
    virtual FOnHktTargetChanged& OnTargetChanged() override { return TargetChangedDelegate; }
    virtual FOnHktCommandChanged& OnCommandChanged() override { return CommandChangedDelegate; }

    // === IHktPlayerInteractionInterface ===
    virtual void ExecuteCommand(UObject* CommandData) override;
    virtual bool GetWorldState(const FHktWorldState*& OutState) const override;
    virtual FOnHktWorldViewUpdated& OnWorldViewUpdated() override { return WorldViewUpdatedDelegate; }
    virtual FOnHktWheelInput& OnWheelInput() override { return WheelInputDelegate; }
    virtual FOnHktSubjectChanged& OnSubjectChanged() override { return SubjectChangedDelegate; }
    virtual FOnHktIntentSubmitted& OnIntentSubmitted() override { return IntentSubmittedDelegate; }
    virtual FOnHktSlotBindingChanged& OnSlotBindingChanged() override { return SlotBindingChangedDelegate; }

    // === Player UID ===
    virtual int64 GetPlayerUid() const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupInputComponent() override;
    virtual void OnRep_PlayerState() override;

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
    TArray<TObjectPtr<UInputAction>> SlotInputActions;

private:
    FOnHktSubjectChanged SubjectChangedDelegate;
    FOnHktTargetChanged TargetChangedDelegate;
    FOnHktCommandChanged CommandChangedDelegate;
    FOnHktIntentSubmitted IntentSubmittedDelegate;
    FOnHktWheelInput WheelInputDelegate;
    FOnHktWorldViewUpdated WorldViewUpdatedDelegate;
    FOnHktSlotBindingChanged SlotBindingChangedDelegate;

    /** 클라이언트 규칙 (Subsystem 소유, 수명 동일) */
    IHktClientRule* CachedClientRule = nullptr;

    /** 가방 컴포넌트 캐시 */
    UHktBagComponent* CachedBagComponent = nullptr;

    /** 캐싱된 인터페이스 포인터들 */
    IHktIntentBuilder* CachedIntentBuilder = nullptr;
    IHktUnitSelectionPolicy* CachedSelectionPolicy = nullptr;
    IHktProxySimulator* CachedProxySimulator = nullptr;
    IHktCommandContainer* CachedCommandContainer = nullptr;
    IHktWorldPlayer* CachedWorldPlayer = nullptr;

    /** OwnedPlayerUid가 일치하는 첫 번째 엔티티 — 기본 Subject */
    FHktEntityId DefaultSubjectEntityId = InvalidEntityId;
    bool bIsInitialSync = false;

    /** WorldState에서 나의 엔티티를 찾아 DefaultSubjectEntityId로 설정 */
    void ResolveDefaultSubject();

    /** 캐릭터 엔티티의 ItemSlot0~8 프로퍼티에서 아이템 스킬을 읽어 CommandSlot에 바인딩 */
    void SyncSlotBindingsFromWorldState(const FHktWorldView& View);

#if ENABLE_HKT_INSIGHTS
    /** Insight 통계 카운터 */
    int32 InsightSentIntentCount = 0;
    int32 InsightReceivedBatchCount = 0;
    int32 InsightReceivedInitialStateCount = 0;
#endif
};
