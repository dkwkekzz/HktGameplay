// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktClientRuleInterfaces.h"
#include "HktCommandContainerComponent.generated.h"

class UHktInputAction;

/**
 * UHktCommandContainerComponent - IHktCommandContainer 구현
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - Actor(PlayerController)는 이 컴포넌트를 Rule에 IHktCommandContainer로 전달
 *
 * 역할:
 *   - SlotActions(UHktInputAction 배열)를 IHktCommandContainer 인터페이스로 래핑
 *   - 슬롯별 EventTag 및 TargetRequired 조회 제공
 */
/** 동적 슬롯 오버라이드 데이터 (아이템 장착에 의해 설정) */
struct FHktSlotOverride
{
    FGameplayTag EventTag;
    bool bTargetRequired = false;
    bool bActive = false;
};

UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktCommandContainerComponent : public UActorComponent, public IHktCommandContainer
{
    GENERATED_BODY()

public:
    UHktCommandContainerComponent();

    // === IHktCommandContainer 구현 ===

    virtual FGameplayTag GetEventTagAtSlot(int32 SlotIndex) const override;
    virtual bool IsTargetRequiredAtSlot(int32 SlotIndex) const override;
    virtual int32 GetNumSlots() const override;
    virtual void SetSlotActions(const TArray<TObjectPtr<UObject>>& InSlotActions) override;
    virtual void OverrideSlotBinding(int32 SlotIndex, FGameplayTag EventTag, bool bTargetRequired) override;
    virtual FOnSlotBindingChanged& OnSlotBindingChanged() override { return SlotBindingChangedDelegate; }

private:
    UPROPERTY()
    TArray<TObjectPtr<UHktInputAction>> SlotActions;

    /** 아이템 장착에 의한 동적 오버라이드 (SlotActions보다 우선) */
    TArray<FHktSlotOverride> SlotOverrides;

    FOnSlotBindingChanged SlotBindingChangedDelegate;
};
