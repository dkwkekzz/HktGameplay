// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktBatchBuilderComponent.h"
#include "Containers/ArrayView.h"
#include <atomic>

UHktBatchBuilderComponent::UHktBatchBuilderComponent()
    : PayloadWriteOffset(0)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHktBatchBuilderComponent::ResetFast(int32 NumGroups, int32 MaxTotalPlayers)
{
    // 1. 그룹 관련 초기화 (기존 유지)
    if (GroupFrameBatches.Num() != NumGroups)
    {
        GroupFrameBatches.SetNum(NumGroups);
        GroupNewbieOwners.SetNum(NumGroups);
    }

    for (int32 i = 0; i < NumGroups; ++i)
    {
        GroupFrameBatches[i].Reset();
        GroupNewbieOwners[i].Reset();
    }

    // 2. [핵심] 페이로드 버퍼 초기화
    // 배열의 크기를 줄이지 않고(Shrink 방지), 카운터만 0으로 돌립니다.
    // MaxTotalPlayers만큼의 Capacity를 미리 확보합니다.
    if (GlobalPayloads.Num() < MaxTotalPlayers)
    {
        GlobalPayloads.SetNumUninitialized(MaxTotalPlayers);
    }
    
    // 원자 변수 0으로 리셋 (락 없이 고속 초기화)
    PayloadWriteOffset.store(0, std::memory_order_relaxed);
}

int32 UHktBatchBuilderComponent::ClaimPayloadSlots(int32 Count)
{
    // fetch_add는 이전 값을 반환하므로, 현재의 Offset이 나의 시작 인덱스가 됨
    return PayloadWriteOffset.fetch_add(Count, std::memory_order_relaxed);
}

TArrayView<const FHktFrameSendPayload> UHktBatchBuilderComponent::GetValidPayloads() const
{
    int32 Count = PayloadWriteOffset.load(std::memory_order_relaxed);
    // 안전 장치: 실제 배열 크기보다 클 경우 (Overflow) 클램핑
    if (Count > GlobalPayloads.Num()) Count = GlobalPayloads.Num();
    
    return MakeArrayView(GlobalPayloads.GetData(), Count);
}

FHktSimulationEvent& UHktBatchBuilderComponent::CreateOrGetGroupFrameBatch(int32 InGroupIdx)
{
    if (!GroupFrameBatches.IsValidIndex(InGroupIdx)) GroupFrameBatches.SetNum(InGroupIdx + 1);
    return GroupFrameBatches[InGroupIdx];
}

TArray<FHktFrameSendPayload>& UHktBatchBuilderComponent::GetMutablePayloads()
{
	return GlobalPayloads;
}

TArray<int64>& UHktBatchBuilderComponent::GetMutableNewbieOwners(int32 InGroupIdx)
{
    if (!GroupNewbieOwners.IsValidIndex(InGroupIdx)) GroupNewbieOwners.SetNum(InGroupIdx + 1);
    return GroupNewbieOwners[InGroupIdx];
}
