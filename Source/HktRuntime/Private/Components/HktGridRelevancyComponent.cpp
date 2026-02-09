// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGridRelevancyComponent.h"
#include "HktInGamePlayerController.h"
#include "HktMasterStashComponent.h"

// ============================================================================
// FHktWorldPlayerAdapter - IHktWorldPlayer 구현
// ============================================================================

FHktWorldPlayerAdapter::FHktWorldPlayerAdapter(AHktInGamePlayerController* InPC, int64 InPlayerUid)
    : PC(InPC), PlayerUid(InPlayerUid)
{
}

void FHktWorldPlayerAdapter::SendFrameBatch(const FHktFrameBatch& Batch)
{
    if (AHktInGamePlayerController* StrongPC = PC.Get())
    {
        StrongPC->Client_ReceiveFrameBatch(Batch);
    }
}

void FHktWorldPlayerAdapter::SendInitialSimulationState(const FHktGroupSimulationState& InitialState)
{
    if (AHktInGamePlayerController* StrongPC = PC.Get())
    {
        StrongPC->Client_ReceiveInitialState(InitialState);
    }
}

// ============================================================================
// FHktRelevancyGroupImpl
// ============================================================================

IHktSimulator& FHktRelevancyGroupImpl::GetSimulator()
{
    check(Simulator);
    return *Simulator;
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

void FHktRelevancyGroupImpl::ClearCaches()
{
    PlayerUids.Reset();
    CachedPlayers.Reset();
}

// ============================================================================
// UHktGridRelevancyComponent
// ============================================================================

UHktGridRelevancyComponent::UHktGridRelevancyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHktGridRelevancyComponent::BeginPlay()
{
    Super::BeginPlay();

    // 기본 그룹 초기화
    Groups.SetNum(FMath::Max(NumGroups, 1));
}

void UHktGridRelevancyComponent::SetMasterStash(UHktMasterStashComponent* InMasterStash)
{
    MasterStash = InMasterStash;
}

// ============================================================================
// IHktRelevancyGraph 구현
// ============================================================================

void UHktGridRelevancyComponent::RegisterPlayer(IHktWorldPlayer* Player, int32 GroupIndex)
{
    if (!Player) return;

    if (Groups.IsValidIndex(GroupIndex))
    {
        Groups[GroupIndex].AddPlayer(Player->GetPlayerUid(), Player);
    }
}

void UHktGridRelevancyComponent::UnregisterPlayer(int64 PlayerUid)
{
    for (FHktRelevancyGroupImpl& Group : Groups)
    {
        Group.RemovePlayer(PlayerUid);
    }
    WorldPlayers.Remove(PlayerUid);
}

void UHktGridRelevancyComponent::UpdateRelevancy()
{
    // 무효한 WorldPlayer 정리
    TArray<int64> ToRemove;
    for (auto& Pair : WorldPlayers)
    {
        if (!Pair.Value || !Pair.Value->IsValid())
        {
            ToRemove.Add(Pair.Key);
        }
    }
    for (int64 Uid : ToRemove)
    {
        UnregisterPlayer(Uid);
    }

    // TODO: 셀 변경에 따른 그룹 재배치 (현재는 단일 그룹 모드)
}

IHktWorldPlayer* UHktGridRelevancyComponent::GetWorldPlayer(int64 PlayerUid) const
{
    if (const TUniquePtr<FHktWorldPlayerAdapter>* Found = WorldPlayers.Find(PlayerUid))
    {
        return Found->Get();
    }
    return nullptr;
}

int32 UHktGridRelevancyComponent::GetGroupIndexByLocation(const FVector& Location) const
{
    // 단순 구현: 셀 → 그룹 매핑
    // 단일 그룹 모드에서는 항상 0
    if (Groups.Num() <= 1) return 0;

    FIntPoint Cell = LocationToCell(Location);
    // 그룹 인덱스 = 해시 % 그룹 수
    return FMath::Abs(HashCombine(GetTypeHash(Cell.X), GetTypeHash(Cell.Y))) % Groups.Num();
}

int32 UHktGridRelevancyComponent::NumRelevancyGroup() const
{
    return Groups.Num();
}

IHktRelevancyGroup& UHktGridRelevancyComponent::GetRelevancyGroup(int32 Index)
{
    return Groups[Index];
}

const IHktRelevancyGroup& UHktGridRelevancyComponent::GetRelevancyGroup(int32 Index) const
{
    return Groups[Index];
}

// ============================================================================
// WorldPlayer 어댑터 관리
// ============================================================================

IHktWorldPlayer* UHktGridRelevancyComponent::RegisterAndWrapClient(AHktInGamePlayerController* Client)
{
    if (!Client) return nullptr;

    // PlayerUid 생성 (PlayerState 기반)
    int64 PlayerUid = 0;
    if (Client->PlayerState)
    {
        FUniqueNetIdRepl UniqueId = Client->PlayerState->GetUniqueId();
        if (UniqueId.IsValid())
        {
            PlayerUid = GetTypeHash(UniqueId->ToString());
        }
    }
    if (PlayerUid == 0)
    {
        PlayerUid = GetTypeHash(Client->GetName());
    }

    // 어댑터 생성
    TUniquePtr<FHktWorldPlayerAdapter> Adapter = MakeUnique<FHktWorldPlayerAdapter>(Client, PlayerUid);
    IHktWorldPlayer* Result = Adapter.Get();

    WorldPlayers.Add(PlayerUid, MoveTemp(Adapter));
    PCToUidMap.Add(Client, PlayerUid);

    UE_LOG(LogTemp, Log, TEXT("[GridRelevancy] Registered client %s as WorldPlayer (Uid=%lld)"),
        *Client->GetName(), PlayerUid);

    return Result;
}

void UHktGridRelevancyComponent::UnregisterClient(AHktInGamePlayerController* Client)
{
    if (!Client) return;

    if (int64* UidPtr = PCToUidMap.Find(Client))
    {
        int64 Uid = *UidPtr;
        UnregisterPlayer(Uid);
        PCToUidMap.Remove(Client);

        UE_LOG(LogTemp, Log, TEXT("[GridRelevancy] Unregistered client %s (Uid=%lld)"),
            *Client->GetName(), Uid);
    }
}

IHktWorldPlayer* UHktGridRelevancyComponent::FindWorldPlayer(AHktInGamePlayerController* Client) const
{
    if (const int64* UidPtr = PCToUidMap.Find(Client))
    {
        return GetWorldPlayer(*UidPtr);
    }
    return nullptr;
}

IHktWorldPlayer* UHktGridRelevancyComponent::FindWorldPlayerByUid(int64 PlayerUid) const
{
    return GetWorldPlayer(PlayerUid);
}

IHktWorldPlayer* UHktGridRelevancyComponent::CreateWorldPlayerFromRecord(const FHktPlayerRecord& Record)
{
    // OnTick_ProcessPendingConnections의 PlayerFactory에서 호출
    // Record에 해당하는 기존 WorldPlayer를 반환 (이미 PostLogin에서 생성됨)
    return GetWorldPlayer(Record.PlayerUid);
}

// ============================================================================
// 셀 유틸
// ============================================================================

FIntPoint UHktGridRelevancyComponent::LocationToCell(const FVector& Location) const
{
    return FIntPoint(
        FMath::FloorToInt(Location.X / CellSize),
        FMath::FloorToInt(Location.Y / CellSize)
    );
}
