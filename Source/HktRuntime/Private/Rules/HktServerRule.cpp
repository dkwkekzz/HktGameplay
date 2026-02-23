// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerRule.h"
#include "HktSimulator.h"
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

void FHktDefaultServerRule::OnReceived_FireIntentEvent(const FHktEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktRelevancyGraph& InGraph, IHktSimulationEventBuilder& InBuilder)
{
    const int32 GroupIndex = InGraph.GetRelevancyGroupIndex(InPlayer.GetPlayerUid());
    InBuilder.PushIntent(GroupIndex, InEvent);
}

void FHktDefaultServerRule::OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB)
{
    const int64 PlayerUid = InPlayer.GetPlayerUid();
    TWeakInterfacePtr<IHktWorldPlayer> WeakPlayer(const_cast<IHktWorldPlayer*>(&InPlayer));

    InDB.LoadPlayerRecordAsync(PlayerUid, [this, WeakPlayer](const FHktPlayerRecord& Record)
    {
        if (Record.IsValid())
        {
            PendingLoginResults.Enqueue({ WeakPlayer, &Record });
        }
    });
}

void FHktDefaultServerRule::OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB)
{
    PendingLogoutRequests.Enqueue(InPlayer.GetPlayerUid());
}

void FHktDefaultServerRule::OnTick_ProcessReady(IHktFrameManager& InFrame)
{
    InFrame.AdvanceFrame();
}

void FHktDefaultServerRule::OnTick_ProcessPendingConnections(
    IHktRelevancyGraph& InGraph, IHktSimulationEventBuilder& InBuilder, IHktWorldDatabase& InDB)
{
    int64 LogoutUid;
    while (PendingLogoutRequests.Dequeue(LogoutUid))
    {
        IHktRelevancyGroup* Group = InGraph.GetRelevancyGroupByPlayer(LogoutUid);
        if (Group)
        {
            IHktAuthoritySimulator& GroupSimulator = Group->GetSimulator();
            InDB.SavePlayerRecordAsync(LogoutUid, GroupSimulator.ExportPlayerState(LogoutUid));
        }

        InGraph.UnregisterPlayer(LogoutUid);
    }

    FPendingLoginResult LoginResult;
    while (PendingLoginResults.Dequeue(LoginResult))
    {
        IHktWorldPlayer* NewPlayer = LoginResult.WeakPlayer.Get();
        if (NewPlayer == nullptr) continue;

        const FHktPlayerRecord& Record = *LoginResult.Record;
        const int64 PlayerUid = Record.PlayerUid;

        // TODO: RegisterPlayer전에 groupidx를 정할 수는 없다.
        const int32 StartGroupIdx = InGraph.GetRelevancyGroupIndex(PlayerUid);
        InGraph.RegisterPlayer(NewPlayer, StartGroupIdx);
        
        InBuilder.EnterWorldPlayer(StartGroupIdx, PlayerUid);
    }

    InGraph.UpdateRelevancy();
}

void FHktDefaultServerRule::OnTick_ProcessSimulationAndPayloads(
    float InDeltaTime,
    const IHktFrameManager& InFrame,
    const IHktRelevancyGraph& InGraph,
    IHktSimulationEventBuilder& InOutBuilder,
    const IHktWorldDatabase& InDB)
{
    const int32 NumGroups = InGraph.NumRelevancyGroup();
	const int64 CurrentFrameNumber = InFrame.GetFrameNumber();

    LastFrameDiffs.SetNum(NumGroups);

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

        // 2. 신규 진입 유저: 엔터티만 import, ActiveEvents는 PushIntent로 주입 (GetIntents 전에 수행)
        TArray<int64>& NewbieOwners = InOutBuilder.GetMutableNewbieOwners(GroupIndex);
        InOutBuilder.GetEnteredPlayers(GroupIndex, NewbieOwners);

        IHktAuthoritySimulator& GroupSimulator = const_cast<IHktRelevancyGroup&>(Group).GetSimulator();
        for (int64 PlayerUid : NewbieOwners)
        {
            if (const FHktPlayerRecord* Rec = InDB.GetCachedPlayerRecord(PlayerUid))
            {
                GroupSimulator.ImportEntityStates(Rec->EntityStates);
                for (const FHktEvent& E : Rec->ActiveEvents)
                {
                    InOutBuilder.PushIntent(GroupIndex, E);
                }
            }
        }

        // 3. 그룹 Intent 수집 (Input Processing, 신규 진입자 ActiveEvents 포함)
        InOutBuilder.GetIntents(GroupIndex, GroupBatch.Events);

        // 4. 나간 유저 처리
        InOutBuilder.GetExitedPlayers(GroupIndex, GroupBatch.RemovedOwnerIds);

        // 5. 시뮬레이터 실행 (State Update) 및 Diff 수집
        LastFrameDiffs[GroupIndex] = GroupSimulator.AdvanceFrame(GroupBatch);
        
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
            NewbieState = &GroupSimulator.GetWorldState();
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
                Payload.DiffToSend = nullptr;
            }
            else
            {
                Payload.StateToSend = nullptr;
                Payload.DiffToSend = &LastFrameDiffs[GroupIndex];
            }
        }
    });
}