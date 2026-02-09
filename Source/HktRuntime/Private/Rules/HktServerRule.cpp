// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerRule.h"
#include "GameplayTagsManager.h"

static int32 HashCombine(int64 A, int32 B) { return (int32)(A * 2654435761) ^ B; }

FHktDefaultServerRule::FHktDefaultServerRule()
{
}

FHktDefaultServerRule::~FHktDefaultServerRule()
{
}

// --- 인증 수신: Authenticator를 통해 인증 수행 ---
void FHktDefaultServerRule::OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback)
{
    // 기본 구현: Authenticator에 인증 요청
    Authenticator.Authenticate(InPrincipal.GetLoginID(), InPrincipal.GetLoginPW(), InResultCallback);
}

// --- Intent 수신 ---
void FHktDefaultServerRule::OnReceived_FireIntentEvent(const FHktIntentEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector)
{
    InCollector.PushIntent(InPlayer.GetPlayerUid(), InEvent);
}

// --- 로그인: Relevancy에 플레이어 등록, DB에서 엔티티 로드/스폰, 스폰 이벤트 발행 ---
void FHktDefaultServerRule::OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB)
{
	const int64 PlayerUid = InPlayer->GetPlayerUid();
    
    // InPlayer 포인터는 람다 캡처로 전달 (PostLogin 시점의 객체가 유효하다고 가정)
    InDB.LoadPlayerRecordAsync(PlayerUid, [this, InPlayer](TUniquePtr<FHktPlayerRecord> RecordPtr)
    {
        // DB 로드 완료 (Worker Thread)
        if (RecordPtr.IsValid())
        {
            // 플레이어 객체와 로드된 기록을 함께 큐에 넣음
            PendingLoginResults.Enqueue({ InPlayer, MoveTemp(RecordPtr) });
        }
    });
}

// --- 로그아웃: 엔티티 저장 및 해제, Relevancy 그래프에서 플레이어 해제 ---
void FHktDefaultServerRule::OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB)
{
	// TODO: 안전하지 않은 저장 방식...
	InDB.SavePlayerRecordAsync(InPlayer.MakePlayerRecord());

	PendingLogoutRequests.Enqueue(InPlayer.GetPlayerUid());
}

// [상용 최적화] 수시 저장 요청 (Game Logic or Timer)
// - 중복 요청(Debouncing) 처리 적용
// - 여러 스레드에서 호출 가능하므로 Lock 사용
void FHktDefaultServerRule::OnEvent_RequestAutosave(int64 PlayerUid)
{
    FScopeLock Lock(&AutosaveQueueLock);
    
    // 이미 대기열에 있다면 추가하지 않음 (이전 요청이 처리될 때 최신 상태가 저장되므로 안전)
    if (!QueuedAutosaveUids.Contains(PlayerUid))
    {
        QueuedAutosaveUids.Add(PlayerUid);
        PendingAutosaveRequests.Enqueue(PlayerUid);
    }
}

    // ------------------------------------------------------------------------
    // [Phase 2] 메인 스레드 프레임 시작 전 처리 (Pre-Tick)
    // ------------------------------------------------------------------------
void FHktDefaultServerRule::OnTick_ProcessPendingConnections(
    IHktRelevancyGraph& InGraph,
    IHktIntentCollector& InCollector,
    IHktWorldDatabase& InDB,
    TFunction<IHktWorldPlayer*(const FHktPlayerRecord&)> PlayerFactory)
{
    // 1. 로그인 처리 (DB 로드 완료된 유저들)
	FPendingLoginResult LoginResult;
	while (PendingLoginResults.Dequeue(LoginResult)) // Dequeue 시 구조체 복사 (UniquePtr 이동)
	{
		if (!LoginResult.Record.IsValid() || !LoginResult.Player) continue;

		IHktWorldPlayer* NewPlayer = LoginResult.Player;
		const FHktPlayerRecord& Record = *LoginResult.Record;

		// 이미 생성된 플레이어를 해당 위치의 그룹에 등록
		const int32 StartGroupIdx = InGraph.GetGroupIndexByLocation(Record.LastPosition);

		InGraph.RegisterPlayer(NewPlayer, StartGroupIdx);
		InCollector.EnterWorldPlayer(StartGroupIdx, Record.PlayerUid);

		if (Record.PendingEvents.Num() > 0)
		{
			InCollector.PushIntents(Record.PlayerUid, Record.PendingEvents);
		}
	}

    // 2. 로그아웃 처리
    int64 LogoutUid;
    while (PendingLogoutRequests.Dequeue(LogoutUid))
    {
        IHktWorldPlayer* Player = InGraph.GetWorldPlayer(LogoutUid);
        if (!Player) continue;

        // [최적화] MakePlayerRecord는 내부 TArray를 RVO로 반환
        // SavePlayerRecordAsync에 MoveTemp로 전달하여 내부 버퍼 포인터만 이동시킴 (복사 X)
        FHktPlayerRecord SaveRecord = Player->MakePlayerRecord();
        InDB.SavePlayerRecordAsync(MoveTemp(SaveRecord));

        InGraph.UnregisterPlayer(LogoutUid);
    }

    // 3. [상용 최적화] 수시 저장 처리 (Autosave)
    int64 AutosaveUid;
    int32 ProcessCount = 0;
    const int32 MaxAutosavePerFrame = 20; // 프레임당 처리량 제한 (서버 틱 유지)

    while (ProcessCount < MaxAutosavePerFrame && PendingAutosaveRequests.Dequeue(AutosaveUid))
    {
        // 처리 시작: Set에서 제거하여 다음 요청을 받을 준비 (Lock 보호)
        {
            FScopeLock Lock(&AutosaveQueueLock);
            QueuedAutosaveUids.Remove(AutosaveUid);
        }

        IHktWorldPlayer* Player = InGraph.GetWorldPlayer(AutosaveUid);
        if (!Player) continue; // 이미 나간 유저면 무시

        // 현재 상태 스냅샷 생성 (Main Thread 안전함)
        FHktPlayerRecord SaveRecord = Player->MakePlayerRecord();
        
        // 비동기 저장 요청 (MoveTemp로 소유권 이전)
        InDB.SavePlayerRecordAsync(MoveTemp(SaveRecord));
        
        ProcessCount++;
    }
}

void FHktDefaultServerRule::OnTick_ExecuteFrame(
	const IHktPersistentFrame& InFrame, 
	const IHktRelevancyGraph& InGraph, 
	IHktIntentCollector& InCollector, 
	IHktBatchBuilder& OutBuilder)
{
	const int32 NumGroups = InGraph.NumRelevancyGroup();
	const int64 CurrentFrameNumber = InFrame.GetFrameNumber();
	const float DeltaTime = InFrame.GetDeltaSeconds();

	ParallelFor(NumGroups, [&](int32 GroupIndex)
	{
		// 1. [Input 구성] 이번 프레임에 시뮬레이션할 재료 준비
		FHktFrameBatch& GroupBatch = OutBuilder.CreateOrGetGroupFrameBatch(GroupIndex);
		GroupBatch.FrameNumber = CurrentFrameNumber;
		GroupBatch.DeltaSeconds = DeltaTime;
		GroupBatch.RandomSeed = HashCombine(CurrentFrameNumber, GroupIndex);

		const IHktRelevancyGroup& Group = InGraph.GetRelevancyGroup(GroupIndex);

		for (const int64 PlayerUid : Group.GetPlayerUids())
		{
			InCollector.GetIntents(PlayerUid, GroupBatch.Events);
		}
		InCollector.GetExitedPlayers(GroupIndex, GroupBatch.RemovedOwnerIds);

		// 2. [Simulation 실행] 서버의 그룹 상태를 업데이트
		// 시뮬레이터는 GroupBatch(입력)를 사용하여 내부의 GroupSimulationState(결과)를 갱신함
		IHktSimulator& GroupSimulator = const_cast<IHktRelevancyGroup&>(Group).GetSimulator(); 
		GroupSimulator.Execute(GroupBatch);

		// 3. [Result 복제] 신규 유저 처리
		TArray<int64>& NewbieOwners = OutBuilder.GetMutableNewbieOwners(GroupIndex);
		InCollector.GetEnteredPlayers(GroupIndex, NewbieOwners);

		if (NewbieOwners.Num() > 0)
		{
			// [핵심 변경] 시뮬레이션이 끝난 "직후"의 그룹 전체 상태를 통째로 복사
			// Global State를 참조하지 않고, 방금 연산이 끝난 Group 자체의 State를 가져옴
			FHktGroupSimulationState& TargetState = OutBuilder.CreateOrGetNewbieState(GroupIndex);
			TargetState = GroupSimulator.GetSimulationState();
			
			// (방어 코드) State의 FrameNumber가 현재 프레임과 일치하는지 확인
			// TargetState.LastProcessedFrameNumber == CurrentFrameNumber 여야 함
		}
	});
}

void FHktDefaultServerRule::OnTick_SendFrameBatch(
	const IHktRelevancyGraph& InGraph, 
	const IHktBatchBuilder& InBuilder)
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
		
		// 신규 유저용 상태 데이터 (포인터로 접근)
		const FHktGroupSimulationState* NewbieState = bHasNewbies ? InBuilder.GetNewbieState(GroupIndex) : nullptr;

		IHktWorldPlayer* const* PlayerPtrData = CachedPlayers.GetData();
		const int32 NumPlayers = CachedPlayers.Num();

		for (int32 i = 0; i < NumPlayers; ++i)
		{
			if (i + 1 < NumPlayers) FPlatformMisc::Prefetch(PlayerPtrData[i + 1]);

			IHktWorldPlayer* Player = PlayerPtrData[i];
			if (!Player) continue; 

			// [분기 처리]
			// Case A: 신규 유저 -> "결과(State)"를 받음. (이번 프레임 연산 생략)
			if (bHasNewbies && NewbieOwners.Contains(Player->GetPlayerUid()) && NewbieState)
			{
				Player->SendInitialSimulationState(*NewbieState);
			}
			// Case B: 기존 유저 -> "입력(Batch)"을 받음. (직접 시뮬레이션 수행)
			else
			{
				Player->SendFrameBatch(GroupBatch);
			}
		}
	}
}
