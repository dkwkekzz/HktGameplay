// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIntentCollectorComponent.h"

UHktIntentCollectorComponent::UHktIntentCollectorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================================
// IHktIntentCollector 구현
// ============================================================================

void UHktIntentCollectorComponent::PushIntent(int64 InPlayerUid, const FHktEvent& InEvent)
{
    PlayerIntents.FindOrAdd(InPlayerUid).Add(InEvent);
}

void UHktIntentCollectorComponent::PushIntents(int64 InPlayerUid, const TArray<FHktEvent>& InEvents)
{
    PlayerIntents.FindOrAdd(InPlayerUid).Append(InEvents);
}

bool UHktIntentCollectorComponent::GetIntents(int64 InPlayerUid, TArray<FHktEvent>& OutIntents)
{
    // 메인 스레드에서만 호출 (ParallelFor 내부에서는 읽기 전용 스냅샷 사용)
    if (TArray<FHktEvent>* Found = PlayerIntents.Find(InPlayerUid))
    {
        OutIntents.Append(*Found);
        return Found->Num() > 0;
    }
    return false;
}

void UHktIntentCollectorComponent::EnterWorldPlayer(int32 GroupIndex, int64 InPlayerUid)
{
    EnteredPlayers.FindOrAdd(GroupIndex).AddUnique(InPlayerUid);
}

void UHktIntentCollectorComponent::ExitWorldPlayer(int32 GroupIndex, int64 InPlayerUid)
{
    ExitedPlayers.FindOrAdd(GroupIndex).AddUnique(InPlayerUid);
}

bool UHktIntentCollectorComponent::GetEnteredPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids)
{
    if (TArray<int64>* Found = EnteredPlayers.Find(GroupIndex))
    {
        OutPlayerUids.Append(*Found);
        return Found->Num() > 0;
    }
    return false;
}

bool UHktIntentCollectorComponent::GetExitedPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids)
{
    if (TArray<int64>* Found = ExitedPlayers.Find(GroupIndex))
    {
        OutPlayerUids.Append(*Found);
        return Found->Num() > 0;
    }
    return false;
}

// ============================================================================
// 프레임 관리
// ============================================================================

void UHktIntentCollectorComponent::EndFrame()
{
    PlayerIntents.Reset();
    EnteredPlayers.Reset();
    ExitedPlayers.Reset();
}
