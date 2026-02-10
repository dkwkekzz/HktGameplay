// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktBatchBuilderComponent.h"

const TArray<int64> UHktBatchBuilderComponent::EmptyOwners;

UHktBatchBuilderComponent::UHktBatchBuilderComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UHktBatchBuilderComponent::Reset(int32 NumGroups)
{
    GroupBatches.SetNum(NumGroups);
    NewbieOwners.SetNum(NumGroups);
    for (int32 i = 0; i < NumGroups; ++i) { GroupBatches[i].Reset(); NewbieOwners[i].Reset(); }
}

FHktFrameBatch& UHktBatchBuilderComponent::CreateOrGetGroupFrameBatch(int32 InGroupIdx)
{
    if (!GroupBatches.IsValidIndex(InGroupIdx)) GroupBatches.SetNum(InGroupIdx + 1);
    return GroupBatches[InGroupIdx];
}

const FHktFrameBatch& UHktBatchBuilderComponent::GetGroupFrameBatch(int32 InGroupIdx) const
{
    check(GroupBatches.IsValidIndex(InGroupIdx));
    return GroupBatches[InGroupIdx];
}

TArray<int64>& UHktBatchBuilderComponent::GetMutableNewbieOwners(int32 InGroupIdx)
{
    if (!NewbieOwners.IsValidIndex(InGroupIdx)) NewbieOwners.SetNum(InGroupIdx + 1);
    return NewbieOwners[InGroupIdx];
}

const TArray<int64>& UHktBatchBuilderComponent::GetNewbieOwners(int32 InGroupIdx) const
{
    return NewbieOwners.IsValidIndex(InGroupIdx) ? NewbieOwners[InGroupIdx] : EmptyOwners;
}
