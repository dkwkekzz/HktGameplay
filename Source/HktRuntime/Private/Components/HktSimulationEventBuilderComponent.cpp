// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationEventBuilderComponent.h"

UHktSimulationEventBuilderComponent::UHktSimulationEventBuilderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================================
// IHktSimulationEventBuilder
// ============================================================================

void UHktSimulationEventBuilderComponent::Resize(int32 NumGroups)
{
	if (GroupIntents.Num() != NumGroups)
	{
		GroupIntents.SetNum(NumGroups);
	}
	for (int32 i = 0; i < GroupIntents.Num(); ++i)
	{
		GroupIntents[i].Reset();
	}
}

void UHktSimulationEventBuilderComponent::PushIntent(int32 GroupIndex, const FHktEvent& InEvent)
{
	if (GroupIntents.IsValidIndex(GroupIndex))
	{
		GroupIntents[GroupIndex].Add(InEvent);
	}
}

bool UHktSimulationEventBuilderComponent::GetIntents(int32 GroupIndex, TArray<FHktEvent>& OutIntents)
{
	if (!GroupIntents.IsValidIndex(GroupIndex))
	{
		return false;
	}
	OutIntents.Append(MoveTemp(GroupIntents[GroupIndex]));
	return true;
}
