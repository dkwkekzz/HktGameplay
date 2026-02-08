// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerRule.h"
#include "GameplayTagsManager.h"

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
void FHktDefaultServerRule::OnLogin_EnterWorldPlayer(
    const IHktWorldPlayer& InPlayer,
    IHktWorldDatabase& InDB,
    IHktRelevancyGraph& InGraph,
    IHktIntentCollector& InCollector,
    IHktWorldState& InState)
{
	int64 PlayerUid = InPlayer.GetPlayerUid();
	InDB.GetOrCreatePlayerRecord(PlayerUid, [PlayerUid, &InGraph, &InCollector](FHktPlayerRecord& Record)
	{
		InGraph.RegisterPlayer(InPlayer);
		InCollector.PushIntents(PlayerUid, Record.IntentEvents);
		InCollector.PushEntitySnapshots(PlayerUid, Record.EntitySnapshots);
	});
}

// --- 로그아웃: 엔티티 저장 및 해제, Relevancy 그래프에서 플레이어 해제 ---
void FHktDefaultServerRule::OnLogout_ExitWorldPlayer(
    const IHktWorldPlayer& InPlayer,
    IHktWorldDatabase& InDB,
    IHktRelevancyGraph& InGraph,
    IHktIntentCollector& InCollector,
    IHktWorldState& InState)
{
	if (InPlayer.ShouldSavePlayerRecord())
	{
		FHktPlayerRecord Record = InPlayer.MakePlayerRecord();
		InDB.SavePlayerRecord(Record);
	}
	
    // 2. Relevancy 그래프에서 플레이어 해제
    InGraph.UnregisterPlayer(InPlayer);
}

void FHktDefaultServerRule::OnTick_ExecuteFrame(IHktPersistentFrame& InFrame, IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder)
{
	InFrame.AdvanceFrame();

	InGraph.UpdateRelevancy();

	for (int32 i = 0; i < InGraph.GetNumRelevancyGroups(); i++)
	{
        FHktFrameBatch GroupBatch;
		GroupBatch.FrameNumber = InFrame.GetFrameNumber();

		GroupBatch.Events.Append(InCollector.GetAllIntents());

		IHktRelevancyGroup& Group = InGraph.GetRelevancyGroup(i);
		for (const IHktWorldPlayer* Player : Group.GetPlayers())
		{
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

