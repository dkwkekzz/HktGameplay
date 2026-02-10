// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerRule.h"
#include "GameplayTagsManager.h"

static int32 HashCombineHelper(int64 A, int32 B) 
{ 
    return (int32)(A * 2654435761) ^ B; 
}

FHktDefaultServerRule::FHktDefaultServerRule() 
{
}

FHktDefaultServerRule::~FHktDefaultServerRule()
{
}

void FHktDefaultServerRule::OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback)
{
    Authenticator.Authenticate(InPrincipal.GetLoginID(), InPrincipal.GetLoginPW(), InResultCallback);
}

void FHktDefaultServerRule::OnReceived_FireIntentEvent(const FHktIntentEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector)
{
    InCollector.PushIntents(InPlayer.GetPlayerUid(), { InEvent });
}

void FHktDefaultServerRule::OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB)
{
    const int64 PlayerUid = InPlayer.GetPlayerUid();

    InDB.LoadPlayerRecordAsync(PlayerUid, [this, PlayerUid](TUniquePtr<FHktPlayerRecord> RecordPtr)
    {
        if (RecordPtr.IsValid())
        {
            PendingLoginResults.Enqueue({ PlayerUid, MoveTemp(RecordPtr) });
        }
    });
}

void FHktDefaultServerRule::OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB)
{
    PendingLogoutRequests.Enqueue(InPlayer.GetPlayerUid());
}

void FHktDefaultServerRule::OnEvent_RequestAutosave(int64 PlayerUid)
{
    FScopeLock Lock(&AutosaveQueueLock);
    if (!QueuedAutosaveUids.Contains(PlayerUid))
    {
        QueuedAutosaveUids.Add(PlayerUid);
        PendingAutosaveRequests.Enqueue(PlayerUid);
    }
}

void FHktDefaultServerRule::OnTick_ProcessPendingConnections(
    IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldDatabase& InDB,
    TFunction<IHktWorldPlayer*(const FHktPlayerRecord&)> PlayerFactory)
{
    FPendingLoginResult LoginResult;
    while (PendingLoginResults.Dequeue(LoginResult))
    {
        if (!LoginResult.Record.IsValid()) continue;

        IHktWorldPlayer* NewPlayer = InGraph.GetWorldPlayer(LoginResult.PlayerUid);
        if (NewPlayer == nullptr) continue;

        const FHktPlayerRecord& Record = *LoginResult.Record;

        const int32 StartGroupIdx = InGraph.GetGroupIndexByLocation(Record.LastPosition);
        InGraph.RegisterPlayer(NewPlayer, StartGroupIdx);
        InCollector.EnterWorldPlayer(StartGroupIdx, Record.PlayerUid);

        if (Record.IntentEvents.Num() > 0)
        {
            InCollector.PushIntents(Record.PlayerUid, Record.IntentEvents);
        }
    }

    int64 LogoutUid;
    while (PendingLogoutRequests.Dequeue(LogoutUid))
    {
        IHktRelevancyGroup* Group = InGraph.GetRelevancyGroupByPlayer(LogoutUid);
        if (Group)
        {
            IHktSimulator& GroupSimulator = Group->GetSimulator();
            FHktOwnerSimulationState OwnerState = GroupSimulator.GetOwnerSimulationState(LogoutUid);

            FHktPlayerRecord NewRecord;
            NewRecord.PlayerUid = LogoutUid;
            NewRecord.IntentEvents = MoveTemp(OwnerState.ActiveEvents);
            NewRecord.EntitySnapshots = MoveTemp(OwnerState.EntitySnapshots);
            InDB.SavePlayerRecordAsync(NewRecord);
        }

        InGraph.UnregisterPlayer(LogoutUid);
    }

    int64 AutosaveUid;
    int32 ProcessCount = 0;
    const int32 MaxAutosavePerFrame = 20;
    while (ProcessCount < MaxAutosavePerFrame && PendingAutosaveRequests.Dequeue(AutosaveUid))
    {
        FScopeLock Lock(&AutosaveQueueLock);
		// TODO: Autosave 처리
        QueuedAutosaveUids.Remove(AutosaveUid);
        ProcessCount++;
    }
}

void FHktDefaultServerRule::OnTick_ExecuteFrame(float InDeltaTime,
    const IHktFrameManager& InFrame, const IHktRelevancyGraph& InGraph,
    IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder)
{
    const float DeltaTime = InDeltaTime;
    const int32 NumGroups = InGraph.NumRelevancyGroup();
    const int64 CurrentFrameNumber = InFrame.GetFrameNumber();

    ParallelFor(NumGroups, [&](int32 GroupIndex)
    {
        FHktFrameBatch& GroupBatch = OutBuilder.CreateOrGetGroupFrameBatch(GroupIndex);
        GroupBatch.FrameNumber = CurrentFrameNumber;
        GroupBatch.DeltaSeconds = DeltaTime;
        GroupBatch.RandomSeed = HashCombineHelper(CurrentFrameNumber, GroupIndex);

        const IHktRelevancyGroup& Group = InGraph.GetRelevancyGroup(GroupIndex);
        for (const int64 PlayerUid : Group.GetPlayerUids())
        {
            InCollector.GetIntents(PlayerUid, GroupBatch.Events);
        }
        InCollector.GetExitedPlayers(GroupIndex, GroupBatch.RemovedOwnerIds);

        IHktSimulator& GroupSimulator = const_cast<IHktRelevancyGroup&>(Group).GetSimulator();
        GroupSimulator.Execute(GroupBatch);

        TArray<int64>& NewbieOwners = OutBuilder.GetMutableNewbieOwners(GroupIndex);
        InCollector.GetEnteredPlayers(GroupIndex, NewbieOwners);
    });
}

void FHktDefaultServerRule::OnTick_SendFrameBatch(
    const IHktRelevancyGraph& InGraph, const IHktBatchBuilder& InBuilder)
{
    const int32 NumGroups = InGraph.NumRelevancyGroup();

    for (int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
    {
        const FHktFrameBatch& GroupBatch = InBuilder.GetGroupFrameBatch(GroupIndex);
        const IHktRelevancyGroup& Group = InGraph.GetRelevancyGroup(GroupIndex);
        const TArray<IHktWorldPlayer*>& CachedPlayers = Group.GetCachedWorldPlayers();
        if (CachedPlayers.Num() == 0) continue;

        const TArray<int64>& NewbieOwners = InBuilder.GetNewbieOwners(GroupIndex);
        const bool bHasNewbies = NewbieOwners.Num() > 0;

        const FHktGroupSimulationState* NewbieState = nullptr;
        if (bHasNewbies)
        {
            IHktSimulator& Simulator = const_cast<IHktRelevancyGroup&>(Group).GetSimulator();
            NewbieState = &Simulator.GetSimulationState();
        }

        for (int32 i = 0; i < CachedPlayers.Num(); ++i)
        {
            IHktWorldPlayer* Player = CachedPlayers[i];
            if (!Player) continue;

            if (bHasNewbies && NewbieOwners.Contains(Player->GetPlayerUid()) && NewbieState)
            {
                Player->SendInitialSimulationState(*NewbieState);
            }
            else
            {
                Player->SendFrameBatch(GroupBatch);
            }
        }
    }
}
