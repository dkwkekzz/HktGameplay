// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerRule.h"
#include "HktCoreSimulator.h"
#include "HktCoreProperties.h"
#include "GameplayTagsManager.h"
#include "NativeGameplayTags.h"
#include "HktTempMapStoryConfig.h"

// Story 태그 — .cpp 전용 static 정의
UE_DEFINE_GAMEPLAY_TAG_STATIC(Flow_Spawner_GoblinCamp,    "Story.Flow.Spawner.GoblinCamp");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Flow_Spawner_Item_TreeDrop, "Story.Flow.Spawner.Item.TreeDrop");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Flow_Spawner_Wave_Arena,    "Story.Flow.Spawner.Wave.Arena");

// 클라이언트 요청 해석용 태그
UE_DEFINE_GAMEPLAY_TAG_STATIC(Event_Move_ToLocation, "Story.Event.Move.ToLocation");

TArray<FHktTempStoryEntry> HktTempMapStoryConfig::GetSpawnersForGroup(int32 GroupIndex)
{
	TArray<FHktTempStoryEntry> Out;

	// 전역 스토리 — 모든 그룹 공통 (FHktMapData::GlobalStories 대응)
	Out.Add({ Flow_Spawner_GoblinCamp,    1000 + GroupIndex * 500, 1000 });
	Out.Add({ Flow_Spawner_Item_TreeDrop,  1200 + GroupIndex * 500,  800 });

	// 그룹 0 전용 — Region별 스토리 (FHktMapRegion::Stories 대응)
	if (GroupIndex == 0)
	{
		Out.Add({ Flow_Spawner_Wave_Arena, 2000, 2000 });
	}

	return Out;
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
// 클라이언트 요청 수신 — 서버가 WorldState에서 EventTag 해석
// ============================================================================

/** ItemSlot PropertyId 테이블 (서버 슬롯 해석용) */
static constexpr uint16 ServerItemSlotPropertyIds[] =
{
	PropertyId::ItemSlot0, PropertyId::ItemSlot1, PropertyId::ItemSlot2,
	PropertyId::ItemSlot3, PropertyId::ItemSlot4, PropertyId::ItemSlot5,
	PropertyId::ItemSlot6, PropertyId::ItemSlot7, PropertyId::ItemSlot8,
};
static constexpr int32 MaxServerItemSlots = UE_ARRAY_COUNT(ServerItemSlotPropertyIds);

void FHktDefaultServerRule::OnReceived_SlotRequest(
	const FHktSlotRequest& InRequest, const IHktWorldPlayer& InPlayer)
{
	if (!CachedGraph) return;

	const int64 PlayerUid = InPlayer.GetPlayerUid();
	const int32 GroupIndex = CachedGraph->GetRelevancyGroupIndex(PlayerUid);
	if (!PendingGroupIntents.IsValidIndex(GroupIndex)) return;

	// 슬롯 인덱스 범위 검증
	if (InRequest.SlotIndex < 0 || InRequest.SlotIndex >= MaxServerItemSlots) return;

	// 소스 엔티티 소유권 검증
	const IHktRelevancyGroup& Group = CachedGraph->GetRelevancyGroup(GroupIndex);
	const FHktWorldState& WS = Group.GetSimulator().GetWorldState();
	if (!WS.IsValidEntity(InRequest.SourceEntity)) return;
	if (WS.GetOwnerUid(InRequest.SourceEntity) != PlayerUid) return;

	// WorldState에서 슬롯 → 아이템 엔티티 → EventTag 해석
	const FHktEntityId ItemId = WS.GetProperty(InRequest.SourceEntity, ServerItemSlotPropertyIds[InRequest.SlotIndex]);
	if (ItemId == 0 || !WS.IsValidEntity(ItemId)) return;

	const int32 SkillTagNetIndex = WS.GetProperty(ItemId, PropertyId::ItemSkillTag);
	if (SkillTagNetIndex <= 0) return;

	const FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(SkillTagNetIndex));
	if (TagName.IsNone()) return;

	const FGameplayTag SkillTag = FGameplayTag::RequestGameplayTag(TagName, false);
	if (!SkillTag.IsValid()) return;

	// FHktEvent 생성 (기존 파이프라인에 투입)
	FHktEvent Event;
	Event.EventId = ++ServerEventSequence;
	Event.EventTag = SkillTag;
	Event.SourceEntity = InRequest.SourceEntity;
	Event.TargetEntity = InRequest.TargetEntity;
	Event.Location = InRequest.TargetLocation;
	Event.PlayerUid = PlayerUid;
	Event.Param1 = InRequest.SlotIndex;
	PendingGroupIntents[GroupIndex].Add(Event);
}

void FHktDefaultServerRule::OnReceived_MoveRequest(
	const FHktMoveRequest& InRequest, const IHktWorldPlayer& InPlayer)
{
	if (!CachedGraph) return;

	const int64 PlayerUid = InPlayer.GetPlayerUid();
	const int32 GroupIndex = CachedGraph->GetRelevancyGroupIndex(PlayerUid);
	if (!PendingGroupIntents.IsValidIndex(GroupIndex)) return;

	// 소스 엔티티 소유권 검증
	const IHktRelevancyGroup& Group = CachedGraph->GetRelevancyGroup(GroupIndex);
	const FHktWorldState& WS = Group.GetSimulator().GetWorldState();
	if (!WS.IsValidEntity(InRequest.SourceEntity)) return;
	if (WS.GetOwnerUid(InRequest.SourceEntity) != PlayerUid) return;

	// FHktEvent 생성 — Move EventTag 직접 매핑
	FHktEvent Event;
	Event.EventId = ++ServerEventSequence;
	Event.EventTag = Event_Move_ToLocation;
	Event.SourceEntity = InRequest.SourceEntity;
	Event.TargetEntity = InRequest.TargetEntity;
	Event.Location = InRequest.Location;
	Event.PlayerUid = PlayerUid;
	PendingGroupIntents[GroupIndex].Add(Event);
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
