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

void FHktDefaultServerRule::OnReceived_FireIntentEvent(const FHktEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector)
{
    InCollector.PushIntents(InPlayer.GetPlayerUid(), { InEvent });
}

void FHktDefaultServerRule::OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB)
{
    const int64 PlayerUid = InPlayer.GetPlayerUid();
    TWeakInterfacePtr<IHktWorldPlayer> WeakPlayer(const_cast<IHktWorldPlayer*>(&InPlayer));

    InDB.LoadPlayerRecordAsync(PlayerUid, [this, WeakPlayer](TUniquePtr<FHktPlayerRecord> RecordPtr)
    {
        if (RecordPtr.IsValid())
        {
            PendingLoginResults.Enqueue({ WeakPlayer, MoveTemp(RecordPtr) });
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
    IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldDatabase& InDB)
{
    FPendingLoginResult LoginResult;
    while (PendingLoginResults.Dequeue(LoginResult))
    {
        if (!LoginResult.Record.IsValid()) continue;

        IHktWorldPlayer* NewPlayer = LoginResult.WeakPlayer.Get();
        if (NewPlayer == nullptr) continue;

        const FHktPlayerRecord& Record = *LoginResult.Record;

        const int32 StartGroupIdx = InGraph.GetGroupIndexByLocation(Record.LastPosition);
        InGraph.RegisterPlayer(NewPlayer, StartGroupIdx);
        InCollector.EnterWorldPlayer(StartGroupIdx, Record.PlayerUid);

        // Database에서 이미 완전한 Record를 제공하므로 무조건 PushIntents 호출
        InCollector.PushIntents(Record.PlayerUid, Record.ActiveEvents);

        // [추가] EntityStates가 있으면 해당 그룹의 다음 배치에 주입
        if (Record.HasEntities())
        {
            InCollector.PushEntityStates(StartGroupIdx, Record.EntityStates);
        }
    }

    int64 LogoutUid;
    while (PendingLogoutRequests.Dequeue(LogoutUid))
    {
        IHktRelevancyGroup* Group = InGraph.GetRelevancyGroupByPlayer(LogoutUid);
        if (Group)
        {
            IHktServerSimulator& GroupSimulator = Group->GetSimulator();
            FHktRuntimeOwnerState OwnerState = GroupSimulator.GetOwnerState(LogoutUid);

            FHktPlayerRecord NewRecord;
            NewRecord.PlayerUid = LogoutUid;
            NewRecord.ActiveEvents = MoveTemp(OwnerState.ActiveEvents);
            NewRecord.EntityStates = MoveTemp(OwnerState.EntityStates);
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

void FHktDefaultServerRule::OnTick_ProcessSimulationAndPayloads(
    float InDeltaTime,
    const IHktFrameManager& InFrame,
    const IHktRelevancyGraph& InGraph,
    IHktIntentCollector& InCollector,
    IHktBatchBuilder& InOutBuilder)
{
    const int32 NumGroups = InGraph.NumRelevancyGroup();
	const int64 CurrentFrameNumber = InFrame.GetFrameNumber();

    // 1. 고속 초기화 (카운터 리셋)
    InOutBuilder.ResetFast(NumGroups, InGraph.GetWorldPlayerCount());

    ParallelFor(NumGroups, [&](int32 GroupIndex)
    {
        // =========================================================
        // Phase 1: Simulation (Write to Builder->GroupBatches)
        // =========================================================
        
        // 1. GroupBatch 준비 (Builder 내부 메모리)
        FHktSimulationEvent& GroupBatch = InOutBuilder.CreateOrGetGroupFrameBatch(GroupIndex);
        
        GroupBatch.FrameNumber = CurrentFrameNumber;
        GroupBatch.DeltaSeconds = InDeltaTime;
        GroupBatch.RandomSeed = HashCombineHelper(CurrentFrameNumber, GroupIndex);

        const IHktRelevancyGroup& Group = InGraph.GetRelevancyGroup(GroupIndex);
        
        // 2. 플레이어 Intent 수집 (Input Processing)
        // 각 플레이어의 입력을 수집하여 Batch에 추가
        for (const int64 PlayerUid : Group.GetPlayerUids())
        {
            TArray<FHktEvent> NewEvents;
            InCollector.GetIntents(PlayerUid, NewEvents);
            GroupBatch.Events.Append(NewEvents);
        }
        
        // [추가] 복원할 EntityStates 수집
        InCollector.GetEntityStatesToRestore(GroupIndex, GroupBatch.RestoredEntityStates);
        
        // 3. 나간 유저 처리
        InCollector.GetExitedPlayers(GroupIndex, GroupBatch.RemovedOwnerIds);

        // 4. 시뮬레이터 실행 (State Update)
        // GroupSimulator는 상태를 변경하므로 const_cast 필요
        IHktServerSimulator& GroupSimulator = const_cast<IHktRelevancyGroup&>(Group).GetSimulator();
        GroupSimulator.Execute(GroupBatch);

        // 5. 신규 진입 유저 처리 (Phase 2에서 Full State 전송 판단용)
        TArray<int64>& NewbieOwners = InOutBuilder.GetMutableNewbieOwners(GroupIndex);
        InCollector.GetEnteredPlayers(GroupIndex, NewbieOwners);

        
        // =========================================================
        // Phase 2: Lock-Free Payload Injection (Smarter Way)
        // =========================================================
        const TArray<IHktWorldPlayer*>& CachedPlayers = Group.GetCachedWorldPlayers();
        const int32 PlayerCount = CachedPlayers.Num();

        if (PlayerCount == 0) return;

        // [Smart] 1. 필요한 공간을 원자적으로 한 번에 예약 (Reserve)
        // 그룹 내 플레이어 수만큼 인덱스를 밀어버림 (경합 최소화)
        const int32 StartIndex = InOutBuilder.ClaimPayloadSlots(PlayerCount);

		TArray<FHktFrameSendPayload>& GlobalPayloads = InOutBuilder.GetMutablePayloads();
        // [Safety] 버퍼 오버플로우 체크 (매우 중요)
        if (StartIndex + PlayerCount > GlobalPayloads.Num())
        {
            // 로그 출력 또는 Assert. 
            // 실시간 게임 서버라면 여기서 return하여 크래시 방지 후, 다음 프레임에 버퍼 늘리도록 유도
            return; 
        }

        const bool bHasNewbies = NewbieOwners.Num() > 0;
        const FHktWorldState* NewbieState = nullptr;
        if (bHasNewbies)
        {
            // Simulator 접근 등
             NewbieState = &GroupSimulator.GetSimulationState();
        }

        // [Smart] 2. 예약된 공간에 데이터 채우기 (Lock 없음, False Sharing 주의)
        // 다른 스레드는 다른 인덱스 범위에 쓰고 있으므로 안전함
        for (int32 i = 0; i < PlayerCount; ++i)
        {
            IHktWorldPlayer* Player = CachedPlayers[i];
            if (!Player) continue; // nullptr 체크 시 인덱스 밀림 주의 (아래 설명 참조)

            // GlobalPayloads[StartIndex + i] 에 직접 접근
            FHktFrameSendPayload& Payload = GlobalPayloads[StartIndex + i];
            
            Payload.TargetActor = Player->GetOwnerActor();
            Payload.PlayerUid = Player->GetPlayerUid();

            if (bHasNewbies && NewbieOwners.Contains(Player->GetPlayerUid()) && NewbieState)
            {
                Payload.StateToSend = NewbieState;
            }
            else
            {
                Payload.BatchToSend = &GroupBatch;
            }
        }
    });
}