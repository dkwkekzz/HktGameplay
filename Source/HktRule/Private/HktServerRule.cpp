// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerRule.h"
#include "HktCoreSimulator.h"
#include "HktCoreProperties.h"
#include "HktBagTypes.h"
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

// 아이템 이벤트 태그 (Pickup/Activate/Deactivate/Drop)
UE_DEFINE_GAMEPLAY_TAG_STATIC(Event_Item_Pickup,     "Story.Event.Item.Pickup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Event_Item_Activate,   "Story.Event.Item.Activate");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Event_Item_Deactivate, "Story.Event.Item.Deactivate");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Event_Item_Drop,       "Story.Event.Item.Drop");

void FHktDefaultServerRule::OnReceived_ItemRequest(
	const FHktItemRequest& InRequest, const IHktWorldPlayer& InPlayer)
{
	if (!CachedGraph) return;

	const int64 PlayerUid = InPlayer.GetPlayerUid();
	const int32 GroupIndex = CachedGraph->GetRelevancyGroupIndex(PlayerUid);
	if (!PendingGroupIntents.IsValidIndex(GroupIndex)) return;

	// 소스 엔티티(캐릭터) 소유권 검증
	const IHktRelevancyGroup& Group = CachedGraph->GetRelevancyGroup(GroupIndex);
	const FHktWorldState& WS = Group.GetSimulator().GetWorldState();
	if (!WS.IsValidEntity(InRequest.SourceEntity)) return;
	if (WS.GetOwnerUid(InRequest.SourceEntity) != PlayerUid) return;

	// 대상 아이템 엔티티 존재 검증
	if (!WS.IsValidEntity(InRequest.TargetEntity)) return;

	// 액션 타입에 따라 EventTag 결정
	FGameplayTag EventTag;
	switch (InRequest.Action)
	{
	case EHktItemAction::Pickup:     EventTag = Event_Item_Pickup;     break;
	case EHktItemAction::Activate:   EventTag = Event_Item_Activate;   break;
	case EHktItemAction::Deactivate: EventTag = Event_Item_Deactivate; break;
	case EHktItemAction::Drop:       EventTag = Event_Item_Drop;       break;
	default: return;
	}

	FHktEvent Event;
	Event.EventId = ++ServerEventSequence;
	Event.EventTag = EventTag;
	Event.SourceEntity = InRequest.SourceEntity;
	Event.TargetEntity = InRequest.TargetEntity;
	Event.PlayerUid = PlayerUid;
	Event.Param0 = InRequest.Param0;  // Activate: ActionSlot
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
// 가방 요청 수신 — Bag ↔ Entity 전환
// ============================================================================

/** ItemSlot PropertyId 테이블 (가방용) */
static constexpr uint16 BagItemSlotPropertyIds[] =
{
	PropertyId::ItemSlot0, PropertyId::ItemSlot1, PropertyId::ItemSlot2,
	PropertyId::ItemSlot3, PropertyId::ItemSlot4, PropertyId::ItemSlot5,
	PropertyId::ItemSlot6, PropertyId::ItemSlot7, PropertyId::ItemSlot8,
};
static constexpr int32 MaxBagItemSlots = UE_ARRAY_COUNT(BagItemSlotPropertyIds);

/** FHktBagItem → FHktEntityState 변환 (엔티티 복원용) */
static FHktEntityState BagItemToEntityState(const FHktBagItem& InItem, int64 OwnerUid)
{
	FHktEntityState ES;
	ES.Data.SetNumZeroed(PropertyId::MaxCount);
	ES.OwnerUid = OwnerUid;

	ES.Data[PropertyId::ItemId]              = InItem.ItemId;
	ES.Data[PropertyId::AttackPower]         = InItem.AttackPower;
	ES.Data[PropertyId::Defense]             = InItem.Defense;
	ES.Data[PropertyId::Stance]              = InItem.Stance;
	ES.Data[PropertyId::ItemSkillTag]        = InItem.ItemSkillTag;
	ES.Data[PropertyId::SkillCPCost]         = InItem.SkillCPCost;
	ES.Data[PropertyId::SkillTargetRequired] = InItem.SkillTargetRequired;
	ES.Data[PropertyId::RecoveryFrame]       = InItem.RecoveryFrame;
	ES.Data[PropertyId::EntitySpawnTag]      = InItem.EntitySpawnTag;

	// EntitySpawnTag → ClassTag (Tags에 추가)
	if (InItem.EntitySpawnTag > 0)
	{
		FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(
			static_cast<FGameplayTagNetIndex>(InItem.EntitySpawnTag));
		if (!TagName.IsNone())
		{
			FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
			if (Tag.IsValid())
			{
				ES.Tags.AddTag(Tag);
			}
		}
	}

	return ES;
}

/** WorldState에서 아이템 엔티티 프로퍼티를 FHktBagItem으로 스냅샷 */
static FHktBagItem SnapshotEntityToBagItem(const FHktWorldState& WS, FHktEntityId ItemEntity)
{
	FHktBagItem Item;
	Item.ItemId              = WS.GetProperty(ItemEntity, PropertyId::ItemId);
	Item.AttackPower         = WS.GetProperty(ItemEntity, PropertyId::AttackPower);
	Item.Defense             = WS.GetProperty(ItemEntity, PropertyId::Defense);
	Item.Stance              = WS.GetProperty(ItemEntity, PropertyId::Stance);
	Item.ItemSkillTag        = WS.GetProperty(ItemEntity, PropertyId::ItemSkillTag);
	Item.SkillCPCost         = WS.GetProperty(ItemEntity, PropertyId::SkillCPCost);
	Item.SkillTargetRequired = WS.GetProperty(ItemEntity, PropertyId::SkillTargetRequired);
	Item.RecoveryFrame       = WS.GetProperty(ItemEntity, PropertyId::RecoveryFrame);
	Item.EntitySpawnTag      = WS.GetProperty(ItemEntity, PropertyId::EntitySpawnTag);
	return Item;
}

void FHktDefaultServerRule::OnReceived_BagRequest(
	const FHktBagRequest& InRequest, IHktWorldPlayer& InPlayer)
{
	if (!CachedGraph) return;

	const int64 PlayerUid = InPlayer.GetPlayerUid();
	const int32 GroupIndex = CachedGraph->GetRelevancyGroupIndex(PlayerUid);
	if (!PendingGroupIntents.IsValidIndex(GroupIndex)) return;

	// 소스 엔티티(캐릭터) 소유권 검증
	const IHktRelevancyGroup& Group = CachedGraph->GetRelevancyGroup(GroupIndex);
	const FHktWorldState& WS = Group.GetSimulator().GetWorldState();
	if (!WS.IsValidEntity(InRequest.SourceEntity)) return;
	if (WS.GetOwnerUid(InRequest.SourceEntity) != PlayerUid) return;

	switch (InRequest.Action)
	{
	case EHktBagAction::StoreFromSlot:
	{
		// ItemSlot → Bag: 엔티티 프로퍼티 스냅샷 → 가방에 저장 → Deactivate 이벤트
		if (InRequest.ActionSlot < 0 || InRequest.ActionSlot >= MaxBagItemSlots) return;

		const FHktEntityId ItemEntity = WS.GetProperty(InRequest.SourceEntity, BagItemSlotPropertyIds[InRequest.ActionSlot]);
		if (ItemEntity == 0 || !WS.IsValidEntity(ItemEntity)) return;

		// Deactivate 전에 스냅샷 (Deactivate가 엔티티를 파괴하기 때문)
		FHktBagItem BagItem = SnapshotEntityToBagItem(WS, ItemEntity);
		int32 OutBagSlot = -1;
		if (!InPlayer.StoreToBag(BagItem, OutBagSlot)) return;

		// Deactivate 이벤트 발행 (기존 Story가 스탯 차감 + 슬롯 클리어 + 엔티티 정리)
		FHktEvent Event;
		Event.EventId = ++ServerEventSequence;
		Event.EventTag = Event_Item_Deactivate;
		Event.SourceEntity = InRequest.SourceEntity;
		Event.TargetEntity = ItemEntity;
		Event.PlayerUid = PlayerUid;
		PendingGroupIntents[GroupIndex].Add(Event);
		break;
	}
	case EHktBagAction::RestoreToSlot:
	{
		// Bag → ItemSlot: 가방에서 아이템 꺼내기 → 엔티티 생성 + Activate (틱에서 처리)
		if (InRequest.ActionSlot < 0 || InRequest.ActionSlot >= MaxBagItemSlots) return;

		FHktBagItem OutItem;
		if (!InPlayer.TakeFromBag(InRequest.BagSlot, OutItem)) return;

		PendingBagEntitySpawns.Add({ OutItem, PlayerUid, GroupIndex, InRequest.SourceEntity, InRequest.ActionSlot, false });
		break;
	}
	case EHktBagAction::Discard:
	{
		// Bag → Ground: 가방에서 아이템 꺼내기 → 바닥 엔티티 생성 (틱에서 처리)
		FHktBagItem OutItem;
		if (!InPlayer.TakeFromBag(InRequest.BagSlot, OutItem)) return;

		PendingBagEntitySpawns.Add({ OutItem, PlayerUid, GroupIndex, InRequest.SourceEntity, -1, true });
		break;
	}
	default:
		break;
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
	PendingGroupEntityStates.SetNum(NumGroups);
	Result.EventSends.SetNum(NumGroups);

	// 로그아웃 처리 (item 9: ExitWorldPlayer 호출)
	int64 LogoutUid;
	while (PendingLogoutRequests.Dequeue(LogoutUid))
	{
		const int32 GroupIndex = Graph.GetRelevancyGroupIndex(LogoutUid);
		if (GroupIndex != INDEX_NONE)
		{
			// 가방 데이터 내보내기 (DB 저장 전)
			IHktWorldPlayer* WorldPlayer = Graph.GetWorldPlayer(LogoutUid);
			if (WorldPlayer)
			{
				// TODO: FHktPlayerState에 BagItems 필드 추가 후 통합 저장
				// 현재는 ExportPlayerState와 별도로 가방 데이터만 기록
			}

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

		// DB에서 로드한 가방 데이터 복원 + 클라이언트 FullSync
		if (LoginResult.Record.BagItems.Num() > 0)
		{
			NewPlayer->RestoreBagFromRecord(LoginResult.Record.BagItems);
			NewPlayer->SendBagFullSync();
		}

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

	// RestoreToSlot/Discard: 가방에서 꺼낸 아이템을 엔티티로 생성 + Activate 이벤트
	for (const FPendingBagEntitySpawn& Spawn : PendingBagEntitySpawns)
	{
		if (!PendingGroupIntents.IsValidIndex(Spawn.GroupIndex)) continue;

		FHktEntityState ES = BagItemToEntityState(Spawn.Item, Spawn.PlayerUid);

		if (Spawn.bDiscard)
		{
			// Ground 엔티티: ItemState=0 (바닥 상태), 캐릭터 위치에 드롭
			ES.Data[PropertyId::ItemState] = 0;
			const IHktRelevancyGroup& Group = Graph.GetRelevancyGroup(Spawn.GroupIndex);
			const FHktWorldState& WS = Group.GetSimulator().GetWorldState();
			if (WS.IsValidEntity(Spawn.CharacterEntity))
			{
				ES.Data[PropertyId::PosX] = WS.GetProperty(Spawn.CharacterEntity, PropertyId::PosX);
				ES.Data[PropertyId::PosY] = WS.GetProperty(Spawn.CharacterEntity, PropertyId::PosY);
				ES.Data[PropertyId::PosZ] = WS.GetProperty(Spawn.CharacterEntity, PropertyId::PosZ);
			}
		}

		PendingGroupEntityStates[Spawn.GroupIndex].Add(ES);

		if (!Spawn.bDiscard)
		{
			// RestoreToSlot: Activate 이벤트 (엔티티 ID는 시뮬레이터가 할당하므로 Param1에 ActionSlot 전달)
			// 실제 TargetEntity는 ImportEntityState 후 결정되므로,
			// Story에서 "마지막으로 생성된 엔티티"를 참조하는 패턴 사용
			FHktEvent ActivateEvent;
			ActivateEvent.EventId = ++ServerEventSequence;
			ActivateEvent.EventTag = Event_Item_Activate;
			ActivateEvent.SourceEntity = Spawn.CharacterEntity;
			ActivateEvent.TargetEntity = InvalidEntityId; // 시뮬레이터가 할당한 새 엔티티
			ActivateEvent.PlayerUid = Spawn.PlayerUid;
			ActivateEvent.Param0 = Spawn.ActionSlot;
			PendingGroupIntents[Spawn.GroupIndex].Add(ActivateEvent);
		}
	}
	PendingBagEntitySpawns.Reset();

	// 병렬 시뮬레이션 (item 8: diff 캐싱 없음)
	ParallelFor(NumGroups, [&](int32 GroupIndex)
	{
		FGroupEventSend& GroupEventSend = Result.EventSends[GroupIndex];
		FHktSimulationEvent& GroupBatch = GroupEventSend.Batch;

		GroupBatch.FrameNumber = CurrentFrameNumber;
		GroupBatch.DeltaSeconds = InDeltaTime;
		GroupBatch.RandomSeed = HashCombineHelper(CurrentFrameNumber, GroupIndex);
		GroupBatch.NewEvents.Append(MoveTemp(PendingGroupIntents[GroupIndex]));

		// Bag에서 복원된 엔티티 주입
		if (PendingGroupEntityStates.IsValidIndex(GroupIndex))
		{
			GroupBatch.NewEntityStates.Append(MoveTemp(PendingGroupEntityStates[GroupIndex]));
		}

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
