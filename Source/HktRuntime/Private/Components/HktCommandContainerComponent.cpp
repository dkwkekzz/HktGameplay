// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCommandContainerComponent.h"
#include "DataAssets/HktInputAction.h"

UHktCommandContainerComponent::UHktCommandContainerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHktCommandContainerComponent::SetSlotActions(const TArray<TObjectPtr<UHktInputAction>>& InSlotActions)
{
    SlotActions = InSlotActions;
}

// ============================================================================
// IHktCommandContainer 구현
// ============================================================================

FGameplayTag UHktCommandContainerComponent::GetEventTagAtSlot(int32 SlotIndex) const
{
    if (SlotActions.IsValidIndex(SlotIndex) && SlotActions[SlotIndex])
    {
        return SlotActions[SlotIndex]->EventTag;
    }
    return FGameplayTag();
}

bool UHktCommandContainerComponent::IsTargetRequiredAtSlot(int32 SlotIndex) const
{
    if (SlotActions.IsValidIndex(SlotIndex) && SlotActions[SlotIndex])
    {
        return SlotActions[SlotIndex]->TargetType != EHktActionTargetType::None
            && SlotActions[SlotIndex]->TargetType != EHktActionTargetType::Self;
    }
    return false;
}

int32 UHktCommandContainerComponent::GetNumSlots() const
{
    return SlotActions.Num();
}
