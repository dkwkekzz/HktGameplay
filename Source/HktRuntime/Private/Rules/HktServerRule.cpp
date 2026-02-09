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
	const int64 PlayerUid = InPlayer.GetPlayerUid();
	InDB.LoadPlayerRecordAsync(PlayerUid, [this](TUniquePtr<FHktPlayerRecord> RecordPtr)
    {
        // DB 로드 완료 (Worker Thread) -> 포인터 소유권을 큐로 이전 (Move)
        if (RecordPtr.IsValid())
        {
            PendingLoginResults.Enqueue(MoveTemp(RecordPtr));
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
			TargetState = Group.GetCurrentSimulationState();
			
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
				Player->Client_ReceiveInitialState(*NewbieState);
			}
			// Case B: 기존 유저 -> "입력(Batch)"을 받음. (직접 시뮬레이션 수행)
			else
			{
				Player->Client_ReceiveFrameBatch(GroupBatch);
			}
		}
	}
}

void FHktDefaultServerRule::OnTick_ExecuteFrame(IHktPersistentFrame& InFrame, IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder)
{
	InFrame.AdvanceFrame();

	InGraph.UpdateRelevancy();

	TArray<int64> EnteredPlayerUids;
	InCollector.GetEnteredPlayers(EnteredPlayerUids);
	for (int64 PlayerUid : EnteredPlayerUids)
	{
		InGraph.RegisterPlayer(PlayerUid);
	}

	TArray<int64> ExitedPlayerUids;
	InCollector.GetExitedPlayers(ExitedPlayerUids);
	for (int64 PlayerUid : ExitedPlayerUids)
	{
		InGraph.UnregisterPlayer(PlayerUid);
	}

	for (int32 i = 0; i < InGraph.GetNumRelevancyGroups(); i++)
	{
        FHktFrameBatch GroupBatch;
		GroupBatch.FrameNumber = InFrame.GetFrameNumber();

		IHktRelevancyGroup& Group = InGraph.GetRelevancyGroup(i);
		for (const IHktWorldPlayer* Player : Group.GetPlayers())
		{
			const int64 PlayerUid = Player->GetPlayerUid();
			InCollector.GetIntents(PlayerUid, GroupBatch.Events);
			InCollector.GetEntitySnapshots(PlayerUid, GroupBatch.Snapshots);

			FHktFrameBatch PlayerBatch = GroupBatch;
			PlayerBatch.Snapshots.Add(InState.CreateEntitySnapshot(Player->GetEntityId()));
		}

		IHktSimulator& GroupSimulator = Group.GetSimulator();
		GroupSimulator.Execute(GroupBatch);
	}
}

void FHktDefaultServerRule::OnTick_SendFrameBatch(const IHktRelevancyGraph& InGraph, const IHktBatchBuilder& InBuilder)
{

}

// --- 셀 단위 FrameBatch: 해당 셀의 모든 엔티티 스냅샷을 매번 전송. 플레이어별로 호출되므로, 이 플레이어가 관심 있는 셀들을 찾아 셀마다 배치 전송 ---
void FHktDefaultServerRule::OnTick_SendFrameBatch(const IHktRelevancyGraph& InGraph, IHktWorldState& InState, IHktWorldPlayer& InPlayer)
{
    const int32 FrameNumber = InState.GetCurrentFrameNumber();
    const TArray<FHktWorldCell>& Cells = InGraph.GetAllInterestedCells();

    for (const FHktWorldCell& Cell : Cells)
    {
        const TArray<IHktWorldPlayer*>& PlayersInCell = InGraph.GetPlayersByCell(Cell);
        // 이 플레이어가 이 셀에 포함되는지 확인 (포인터 비교)
        bool bPlayerInCell = false;
        for (IHktWorldPlayer* P : PlayersInCell)
        {
            if (P == &InPlayer)
            {
                bPlayerInCell = true;
                break;
            }
        }
        if (!bPlayerInCell)
        {
            continue;
        }

        FHktFrameBatch Batch;
        Batch.FrameNumber = FrameNumber;
        // 명세: 주어진 셀의 모든 엔티티 스냅샷을 매번 보낸다.
        Batch.Snapshots = InState.GetEntitySnapshotsByCell(Cell);
        // 문제: 셀별 이벤트/RemovedEntities는 현재 인터페이스에 없음. GetEventsByCell(Cell), GetRemovedEntitiesByCell(Cell) 등이 필요하면 IHktWorldState 확장 필요.
        // Batch.Events / Batch.RemovedEntities 는 구현체에서 채우거나, 오케스트레이터가 배치에 세팅 후 전달하는 방식으로 보완 가능.

        if (!Batch.IsEmpty())
        {
            InPlayer.SendFrameBatch(Batch);
        }
    }
}

// --- 셀 단위 시뮬레이션 실행: 각 관심 셀에 대해 FrameBatch를 구성해 Execute 호출. (InState가 없어 현재는 프레임 번호만 넣은 배치 1회 호출) ---
void FHktDefaultServerRule::OnTick_ExecuteFrame(const IHktPersistentFrame& InFrame, const IHktRelevancyGraph& InGraph, IHktSimulator& InSimulator)
{
    // 문제: IHktSimulator::Execute(FHktFrameBatch) 는 셀별 배치를 받도록 바뀌었으나, OnTick_ExecuteFrame 시그니처에 IHktWorldState가 없어
    // 셀별 스냅샷/이벤트로 배치를 구성할 수 없음. 오케스트레이터가 (InFrame, InGraph, InState, InSimulator) 로 한 번에 넘기거나,
    // 셀별 Execute 호출은 오케스트레이터에서 GetAllInterestedCells() → GetEntitySnapshotsByCell(Cell) 로 배치 구성 후 Execute(Batch) 호출하는 방식 권장.
    FHktFrameBatch Batch;
    Batch.FrameNumber = static_cast<int32>(InFrame.GetFrameNumber());
    InSimulator.Execute(Batch);
}

