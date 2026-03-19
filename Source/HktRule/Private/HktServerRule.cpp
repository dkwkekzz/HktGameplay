// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerRule.h"
#include "HktCoreSimulator.h"
#include "GameplayTagsManager.h"
#include "NativeGameplayTags.h"
#include "HktTempMapStoryConfig.h"

namespace
{
	int32 HashCombineHelper(int64 A, int32 B)
	{
		return static_cast<int32>(A * 2654435761) ^ B;
	}
}

FHktDefaultServerRule::FHktDefaultServerRule()
{
}

FHktDefaultServerRule::~FHktDefaultServerRule()
{
}

// ============================================================================
// 컨텍스트 바인딩 (item 2)
// ============================================================================

void FHktDefaultServerRule::BindContext(
	IHktFrameManager* InFrame,
	IHktRelevancyGraph* InGraph,
	IHktWorldDatabase* InDB)
{
	CachedFrame   = InFrame;
	CachedGraph   = InGraph;
	CachedDB      = InDB;
}

// ============================================================================
// 인증
// ============================================================================

void FHktDefaultServerRule::OnReceived_Authentication(
	IHktAuthenticator& Authenticator,
	const IHktPrincipal& InPrincipal,
	TFunction<void(bool bSuccess, const FString& Token)> InResultCallback)
{
	Authenticator.Authenticate(InPrincipal.GetLoginID(), InPrincipal.GetLoginPW(), InResultCallback);
}

// ============================================================================
// Intent (item 2 — 내부 캐싱된 Graph/Builder 사용)
// ============================================================================

void FHktDefaultServerRule::OnReceived_FireIntentEvent(
	const FHktEvent& InEvent, const IHktWorldPlayer& InPlayer)
{
	if (!CachedGraph) return;

	const int32 GroupIndex = CachedGraph->GetRelevancyGroupIndex(InPlayer.GetPlayerUid());
	if (PendingGroupIntents.IsValidIndex(GroupIndex))
	{
		FHktEvent Copy = InEvent;
		Copy.EventId = ++ServerEventSequence;
		Copy.PlayerUid = InPlayer.GetPlayerUid();
		PendingGroupIntents[GroupIndex].Add(Copy);
	}
}

// ============================================================================
// 액터 이벤트 (item 1, 2)
// ============================================================================

void FHktDefaultServerRule::OnEvent_GameModePostLogin(const IHktWorldPlayer& InPlayer)
{
	if (!CachedDB) return;

	const int64 PlayerUid = InPlayer.GetPlayerUid();
	TWeakInterfacePtr<IHktWorldPlayer> WeakPlayer(const_cast<IHktWorldPlayer*>(&InPlayer));

	CachedDB->LoadPlayerRecordAsync(PlayerUid, [this, WeakPlayer](const FHktPlayerRecord& Record)
	{
		if (Record.IsValid())
		{
			PendingLoginResults.Enqueue({ WeakPlayer, Record });
		}
	});
}

void FHktDefaultServerRule::OnEvent_GameModeLogout(const IHktWorldPlayer& InPlayer)
{
	// 로그아웃 UID를 큐잉 — ProcessPendingConnections에서 ExitWorldPlayer 포함하여 처리 (item 9)
	PendingLogoutRequests.Enqueue(InPlayer.GetPlayerUid());
}

// ============================================================================
// 틱 (item 1, 2, 3, 4, 5, 6, 8, 9)
// ============================================================================

FHktEventGameModeTickResult FHktDefaultServerRule::OnEvent_GameModeTick(float InDeltaTime)
{
	FHktEventGameModeTickResult Result;

	if (!CachedFrame || !CachedGraph || !CachedDB)
	{
		return Result;
	}

	IHktFrameManager&           Frame   = *CachedFrame;
	IHktRelevancyGraph&         Graph   = *CachedGraph;
	IHktWorldDatabase&          DB      = *CachedDB;

	// --- ProcessReady ---
	Frame.AdvanceFrame();

	// --- ProcessPendingConnections ---
	Graph.UpdateRelevancy();

	const int32 NumGroups = Graph.NumRelevancyGroup();
	const int64 CurrentFrameNumber = Frame.GetFrameNumber();

	PendingGroupIntents.SetNum(NumGroups);
	Result.EventSends.SetNum(NumGroups);

	// 로그아웃 처리 (item 9: ExitWorldPlayer 호출)
	int64 LogoutUid;
	while (PendingLogoutRequests.Dequeue(LogoutUid))
	{
		const int32 GroupIndex = Graph.GetRelevancyGroupIndex(LogoutUid);
		if (GroupIndex != INDEX_NONE)
		{
			IHktRelevancyGroup& Group = Graph.GetRelevancyGroup(GroupIndex);
			IHktAuthoritySimulator& Simulator = Group.GetSimulator();
			DB.SavePlayerRecordAsync(LogoutUid, Simulator.ExportPlayerState(LogoutUid));

			const int32 GroupIdx = Graph.GetRelevancyGroupIndex(LogoutUid);
			FGroupEventSend& GroupEventSend = Result.EventSends[GroupIdx];
			GroupEventSend.Batch.RemovedOwnerIds.Add(LogoutUid);
		}
	}

	// 로그인 처리 — Graph 등록은 EndFrame에서 처리 (item 5)
	FPendingLoginResult LoginResult;
	while (PendingLoginResults.Dequeue(LoginResult))
	{
		IHktWorldPlayer* NewPlayer = LoginResult.WeakPlayer.Get();
		if (!NewPlayer) continue;

		const int32 GroupIdx  = Graph.CalculateRelevancyGroupIndex(LoginResult.Record.LastPosition);
		FGroupEventSend& GroupEventSend = Result.EventSends[GroupIdx];
		GroupEventSend.Entered.Add(NewPlayer);
	}

	// --- Temp Map Story Injection (TODO: MapGenerator의 FHktMapData로 교체) ---
	// HktTempMapStoryConfig에서 테스트용 스토리 목록을 읽어 1회 fire.
	// 향후: FHktMapData::GlobalStories + FHktMapRegion::Stories에서 읽도록 변경.
	if (ActiveSpawnerFlows.Num() == 0)
	{
		for (int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
		{
			for (const FHktTempStoryEntry& Entry : HktTempMapStoryConfig::GetSpawnersForGroup(GroupIndex))
			{
				FHktEvent SpawnerEvent;
				SpawnerEvent.EventId = ++ServerEventSequence;
				SpawnerEvent.EventTag = Entry.StoryTag;
				SpawnerEvent.Param0 = Entry.SpawnPosX;
				SpawnerEvent.Param1 = Entry.SpawnPosY;
				PendingGroupIntents[GroupIndex].Add(SpawnerEvent);
				ActiveSpawnerFlows.Add(Entry.StoryTag);
			}
		}
	}

	// --- ProcessSimulationAndPayloads ---

	// 병렬 시뮬레이션 (item 8: diff 캐싱 없음)
	ParallelFor(NumGroups, [&](int32 GroupIndex)
	{
		FGroupEventSend& GroupEventSend = Result.EventSends[GroupIndex];
		FHktSimulationEvent& GroupBatch = GroupEventSend.Batch;

		GroupBatch.FrameNumber = CurrentFrameNumber;
		GroupBatch.DeltaSeconds = InDeltaTime;
		GroupBatch.RandomSeed = HashCombineHelper(CurrentFrameNumber, GroupIndex);
		GroupBatch.NewEvents.Append(MoveTemp(PendingGroupIntents[GroupIndex]));

		// 신입 엔티티/이벤트 주입
		for (IHktWorldPlayer* NewPlayer : GroupEventSend.Entered)
		{
			if (const FHktPlayerRecord* Rec = DB.GetCachedPlayerRecord(NewPlayer->GetPlayerUid()))
			{
				GroupBatch.NewEntityStates.Append(Rec->EntityStates);
				GroupBatch.NewEvents.Append(Rec->ActiveEvents);
			}
		}

		// item 8: diff 버림 (서버는 Diff 불필요)
		IHktRelevancyGroup& Group = Graph.GetRelevancyGroup(GroupIndex);
		IHktAuthoritySimulator& Simulator = Group.GetSimulator();
		Simulator.AdvanceFrame(GroupBatch);

		GroupEventSend.Existing = &Group.GetCachedWorldPlayers();
		GroupEventSend.NewState = &Simulator.GetWorldState();

		for (int64 PlayerUid : GroupEventSend.Batch.RemovedOwnerIds)
		{
			Graph.UnregisterPlayer(PlayerUid);
		}

		for (IHktWorldPlayer* NewPlayer : GroupEventSend.Entered)
		{
			Graph.RegisterPlayer(NewPlayer, GroupIndex);
		}
	});

	return Result;
}
