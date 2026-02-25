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

private:
    UPROPERTY()
    TArray<TObjectPtr<UHktInputAction>> SlotActions;
};
