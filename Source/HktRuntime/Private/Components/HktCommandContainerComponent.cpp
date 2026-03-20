// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCommandContainerComponent.h"
#include "DataAssets/HktInputAction.h"

UHktCommandContainerComponent::UHktCommandContainerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHktCommandContainerComponent::SetSlotActions(const TArray<TObjectPtr<UObject>>& InSlotActions)
{
    SlotActions.Reset();
    for (UObject* Obj : InSlotActions)
    {
        if (UHktInputAction* Action = Cast<UHktInputAction>(Obj))
        {
            SlotActions.Add(Action);
        }
    }
}

// ============================================================================
// IHktCommandContainer 구현
// ============================================================================

FGameplayTag UHktCommandContainerComponent::GetEventTagAtSlot(int32 SlotIndex) const
{
    // 동적 오버라이드가 있으면 우선
    if (SlotOverrides.IsValidIndex(SlotIndex) && SlotOverrides[SlotIndex].bActive)
    {
        return SlotOverrides[SlotIndex].EventTag;
    }
    if (SlotActions.IsValidIndex(SlotIndex) && SlotActions[SlotIndex])
    {
        return SlotActions[SlotIndex]->EventTag;
    }
    return FGameplayTag();
}

bool UHktCommandContainerComponent::IsTargetRequiredAtSlot(int32 SlotIndex) const
{
    // 동적 오버라이드가 있으면 우선
    if (SlotOverrides.IsValidIndex(SlotIndex) && SlotOverrides[SlotIndex].bActive)
    {
        return SlotOverrides[SlotIndex].bTargetRequired;
    }
    if (SlotActions.IsValidIndex(SlotIndex) && SlotActions[SlotIndex])
    {
        return SlotActions[SlotIndex]->TargetType != EHktActionTargetType::None
            && SlotActions[SlotIndex]->TargetType != EHktActionTargetType::Self;
    }
    return false;
}

int32 UHktCommandContainerComponent::GetNumSlots() const
{
    return FMath::Max(SlotActions.Num(), SlotOverrides.Num());
}

void UHktCommandContainerComponent::OverrideSlotBinding(int32 SlotIndex, FGameplayTag EventTag, bool bTargetRequired)
{
    if (SlotIndex < 0) return;

    // 필요 시 배열 확장
    if (SlotIndex >= SlotOverrides.Num())
    {
        SlotOverrides.SetNum(SlotIndex + 1);
    }

    FHktSlotOverride& Override = SlotOverrides[SlotIndex];
    if (EventTag.IsValid())
    {
        Override.EventTag = EventTag;
        Override.bTargetRequired = bTargetRequired;
        Override.bActive = true;
    }
    else
    {
        // 빈 태그 = 오버라이드 해제
        Override = FHktSlotOverride();
    }

    // 개별 broadcast 하지 않음 — 호출자(PlayerController)가 배치 완료 후 한 번에 broadcast
}
