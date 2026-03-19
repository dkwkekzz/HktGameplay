// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerRule.h"
#include "HktCoreSimulator.h"
#include "GameplayTagsManager.h"
#include "NativeGameplayTags.h"
#include "HktCoreEventLog.h"

namespace HktServerRuleSpawnerTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_Spawner_GoblinCamp, "Story.Flow.Spawner.GoblinCamp", "Periodic goblin camp spawner flow.");
}

namespace HktServerRuleValidationTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Story_Event, "Story.Event", "Client Intent event parent tag.");
}

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

	const int64 PlayerUid = InPlayer.GetPlayerUid();
	const int32 GroupIndex = CachedGraph->GetRelevancyGroupIndex(PlayerUid);
	if (!PendingGroupIntents.IsValidIndex(GroupIndex))
	{
		return;
	}

	// Gap 0: Client Intent 검증 — Story.Event.* 태그는 SourceEntity 소유권 확인 필수
	if (InEvent.EventTag.MatchesTag(HktServerRuleValidationTags::Tag_Story_Event))
	{
		const IHktRelevancyGroup& Group = CachedGraph->GetRelevancyGroup(GroupIndex);
		const FHktWorldState& WorldState = Group.GetSimulator().GetWorldState();

		// SourceEntity가 월드에 존재하는지 확인
		if (!WorldState.IsValidEntity(InEvent.SourceEntity))
		{
			HKT_EVENT_LOG("Rule.Validation",
				FString::Printf(TEXT("Rejected: SourceEntity=%d not valid. PlayerUid=%lld Tag=%s"),
					InEvent.SourceEntity, PlayerUid, *InEvent.EventTag.ToString()));
			return;
		}

		// SourceEntity가 해당 플레이어 소유인지 확인
		const int64 EntityOwnerUid = WorldState.GetOwnerUid(InEvent.SourceEntity);
		if (EntityOwnerUid != PlayerUid)
		{
			HKT_EVENT_LOG("Rule.Validation",
				FString::Printf(TEXT("Rejected: SourceEntity=%d OwnerUid=%lld != PlayerUid=%lld Tag=%s"),
					InEvent.SourceEntity, EntityOwnerUid, PlayerUid, *InEvent.EventTag.ToString()));
			return;
		}
	}

	FHktEvent Copy = InEvent;
	Copy.EventId = ++ServerEventSequence;
	Copy.PlayerUid = PlayerUid;
	PendingGroupIntents[GroupIndex].Add(Copy);
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

	// --- NPC Spawner Event Injection ---
	// Zone 데이터는 향후 Map/MCP에서 로드. 현재는 샘플 스포너 이벤트를 1회 fire.
	// 각 그룹에 스포너를 한 번만 활성화 (VM이 루프하며 자체 관리)
	{
		using namespace HktServerRuleSpawnerTags;
		if (!ActiveSpawnerFlows.Contains(Flow_Spawner_GoblinCamp))
		{
			for (int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
			{
				FHktEvent SpawnerEvent;
				SpawnerEvent.EventId = ++ServerEventSequence;
				SpawnerEvent.EventTag = Flow_Spawner_GoblinCamp;
				SpawnerEvent.Param0 = 1000 + GroupIndex * 500;  // SpawnPosX (샘플)
				SpawnerEvent.Param1 = 1000;                     // SpawnPosY (샘플)
				PendingGroupIntents[GroupIndex].Add(SpawnerEvent);
			}
			ActiveSpawnerFlows.Add(Flow_Spawner_GoblinCamp);
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
