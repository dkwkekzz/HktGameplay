// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktSimulationEventBuilderComponent.h"
#include "Containers/ArrayView.h"

UHktSimulationEventBuilderComponent::UHktSimulationEventBuilderComponent()
    : PayloadWriteOffset(0)
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================================
// 프레임 초기화/정리
// ============================================================================

void UHktSimulationEventBuilderComponent::ResetFast(int32 NumGroups, int32 MaxTotalPlayers)
{
    // 그룹 수 변경 시 모든 그룹 배열을 한 번에 조정
    if (GroupFrameBatches.Num() != NumGroups)
    {
        GroupFrameBatches.SetNum(NumGroups);
        GroupNewbieOwners.SetNum(NumGroups);
        GroupIntents.SetNum(NumGroups);
        EnteredPlayers.SetNum(NumGroups);
        ExitedPlayers.SetNum(NumGroups);
        PendingEntityStates.SetNum(NumGroups);
    }

    for (int32 i = 0; i < NumGroups; ++i)
    {
        GroupFrameBatches[i].Reset();
        GroupNewbieOwners[i].Reset();
        GroupIntents[i].Reset();
    }

    if (GlobalPayloads.Num() < MaxTotalPlayers)
    {
        GlobalPayloads.SetNumUninitialized(MaxTotalPlayers);
    }

    PayloadWriteOffset.store(0, std::memory_order_relaxed);
}

void UHktSimulationEventBuilderComponent::EndFrame()
{
    for (auto& A : GroupIntents)        A.Reset();
    for (auto& A : EnteredPlayers)     A.Reset();
    for (auto& A : ExitedPlayers)      A.Reset();
    for (auto& A : PendingEntityStates) A.Reset();
}

// ============================================================================
// Intent 입력
// ============================================================================

void UHktSimulationEventBuilderComponent::PushIntent(int32 GroupIndex, const FHktEvent& InEvent)
{
    if (!GroupIntents.IsValidIndex(GroupIndex)) GroupIntents.SetNum(GroupIndex + 1);
    GroupIntents[GroupIndex].Add(InEvent);
}

void UHktSimulationEventBuilderComponent::PushIntents(int32 GroupIndex, const TArray<FHktEvent>& InEvents)
{
    if (!GroupIntents.IsValidIndex(GroupIndex)) GroupIntents.SetNum(GroupIndex + 1);
    GroupIntents[GroupIndex].Append(InEvents);
}

bool UHktSimulationEventBuilderComponent::GetIntents(int32 GroupIndex, TArray<FHktEvent>& OutIntents)
{
    if (!GroupIntents.IsValidIndex(GroupIndex)) return false;
    OutIntents.Append(GroupIntents[GroupIndex]);
    return GroupIntents[GroupIndex].Num() > 0;
}

// ============================================================================
// 플레이어 진입/퇴장
// ============================================================================

void UHktSimulationEventBuilderComponent::EnterWorldPlayer(int32 GroupIndex, int64 InPlayerUid)
{
    if (!EnteredPlayers.IsValidIndex(GroupIndex)) EnteredPlayers.SetNum(GroupIndex + 1);
    EnteredPlayers[GroupIndex].AddUnique(InPlayerUid);
}

void UHktSimulationEventBuilderComponent::ExitWorldPlayer(int32 GroupIndex, int64 InPlayerUid)
{
    if (!ExitedPlayers.IsValidIndex(GroupIndex)) ExitedPlayers.SetNum(GroupIndex + 1);
    ExitedPlayers[GroupIndex].AddUnique(InPlayerUid);
}

bool UHktSimulationEventBuilderComponent::GetEnteredPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids)
{
    if (!EnteredPlayers.IsValidIndex(GroupIndex)) return false;
    OutPlayerUids.Append(EnteredPlayers[GroupIndex]);
    return EnteredPlayers[GroupIndex].Num() > 0;
}

bool UHktSimulationEventBuilderComponent::GetExitedPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids)
{
    if (!ExitedPlayers.IsValidIndex(GroupIndex)) return false;
    OutPlayerUids.Append(ExitedPlayers[GroupIndex]);
    return ExitedPlayers[GroupIndex].Num() > 0;
}

// ============================================================================
// EntityState 복원 큐
// ============================================================================

void UHktSimulationEventBuilderComponent::PushEntityStates(int32 GroupIndex, const TArray<FHktEntityState>& InStates)
{
    if (!PendingEntityStates.IsValidIndex(GroupIndex)) PendingEntityStates.SetNum(GroupIndex + 1);
    PendingEntityStates[GroupIndex].Append(InStates);
}

bool UHktSimulationEventBuilderComponent::GetEntityStatesToRestore(int32 GroupIndex, TArray<FHktEntityState>& OutStates)
{
    if (!PendingEntityStates.IsValidIndex(GroupIndex)) return false;
    OutStates.Append(PendingEntityStates[GroupIndex]);
    return PendingEntityStates[GroupIndex].Num() > 0;
}

// ============================================================================
// Batch 조립
// ============================================================================

FHktSimulationEvent& UHktSimulationEventBuilderComponent::CreateOrGetGroupFrameBatch(int32 InGroupIdx)
{
    return GroupFrameBatches[InGroupIdx];
}

int32 UHktSimulationEventBuilderComponent::ClaimPayloadSlots(int32 Count)
{
    return PayloadWriteOffset.fetch_add(Count, std::memory_order_relaxed);
}

TArray<FHktFrameSendPayload>& UHktSimulationEventBuilderComponent::GetMutablePayloads()
{
    return GlobalPayloads;
}

TArray<int64>& UHktSimulationEventBuilderComponent::GetMutableNewbieOwners(int32 InGroupIdx)
{
    return GroupNewbieOwners[InGroupIdx];
}

TArrayView<const FHktFrameSendPayload> UHktSimulationEventBuilderComponent::GetValidPayloads() const
{
    int32 Count = PayloadWriteOffset.load(std::memory_order_relaxed);
    if (Count > GlobalPayloads.Num()) Count = GlobalPayloads.Num();
    return MakeArrayView(GlobalPayloads.GetData(), Count);
}
