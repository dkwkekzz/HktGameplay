// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGridRelevancyComponent.h"

FHktRelevancyGroupImpl::FHktRelevancyGroupImpl() 
    : Simulator(CreateAuthoritySimulator())
{
}

void FHktRelevancyGroupImpl::AddPlayer(int64 Uid, IHktWorldPlayer* Player)
{
    PlayerUids.AddUnique(Uid);
    CachedPlayers.AddUnique(Player);
}

void FHktRelevancyGroupImpl::RemovePlayer(int64 Uid)
{
    int32 Index = PlayerUids.IndexOfByKey(Uid);
    if (Index != INDEX_NONE)
    {
        PlayerUids.RemoveAt(Index);
        CachedPlayers.RemoveAt(Index);
    }
}

bool FHktRelevancyGroupImpl::HasPlayer(int64 Uid) const
{
    return PlayerUids.Contains(Uid);
}

UHktGridRelevancyComponent::UHktGridRelevancyComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UHktGridRelevancyComponent::BeginPlay()
{
    Super::BeginPlay();
    Groups.SetNum(FMath::Max(NumInitialGroups, 1));
}

void UHktGridRelevancyComponent::RegisterPlayer(IHktWorldPlayer* Player, int32 GroupIndex)
{
    if (!Player) return;
    const int64 Uid = Player->GetPlayerUid();
    RegisteredPlayers.Add(Uid, Player);
    if (Groups.IsValidIndex(GroupIndex)) { Groups[GroupIndex].AddPlayer(Uid, Player); }
}

void UHktGridRelevancyComponent::UnregisterPlayer(int64 PlayerUid)
{
    for (FHktRelevancyGroupImpl& Group : Groups) { Group.RemovePlayer(PlayerUid); }
    RegisteredPlayers.Remove(PlayerUid);
}

void UHktGridRelevancyComponent::UpdateRelevancy() { /* TODO: 셀 변경에 따른 그룹 재배치 */ }

IHktWorldPlayer* UHktGridRelevancyComponent::GetWorldPlayer(int64 PlayerUid) const
{
    if (IHktWorldPlayer* const* Found = RegisteredPlayers.Find(PlayerUid)) { return *Found; }
    return nullptr;
}

int32 UHktGridRelevancyComponent::NumRelevancyGroup() const { return Groups.Num(); }

IHktRelevancyGroup& UHktGridRelevancyComponent::GetRelevancyGroup(int32 Index) { return Groups[Index]; }

const IHktRelevancyGroup& UHktGridRelevancyComponent::GetRelevancyGroup(int32 Index) const { return Groups[Index]; }

IHktRelevancyGroup* UHktGridRelevancyComponent::GetRelevancyGroupByPlayer(int64 PlayerUid)
{
    return const_cast<IHktRelevancyGroup*>(static_cast<const UHktGridRelevancyComponent*>(this)->GetRelevancyGroupByPlayer(PlayerUid));
}

const IHktRelevancyGroup* UHktGridRelevancyComponent::GetRelevancyGroupByPlayer(int64 PlayerUid) const
{
    for (const FHktRelevancyGroupImpl& Group : Groups)
    {
        if (Group.HasPlayer(PlayerUid))
            return &Group;
    }

    return nullptr;
}

FIntPoint UHktGridRelevancyComponent::LocationToCell(const FVector& Location) const
{
    return FIntPoint(FMath::FloorToInt(Location.X / CellSize), FMath::FloorToInt(Location.Y / CellSize));
}

int32 UHktGridRelevancyComponent::GetRelevancyGroupIndex(int64 PlayerUid) const
{
    for (int32 Index = 0; Index < Groups.Num(); ++Index)
    {
        if (Groups[Index].HasPlayer(PlayerUid))
        {
            return Index;
        }
    }
    return 0;
}
