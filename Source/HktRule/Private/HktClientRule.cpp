// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClientRule.h"
#include "HktCoreProperties.h"
#include "HktCoreEventLog.h"

// 기본 액션 태그 (슬롯 미선택 시 타겟 유형별 하드코딩)
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Event_Item_Pickup, "Story.Event.Item.Pickup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Event_Attack_Basic, "Story.Event.Attack.Basic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Event_Move_ToLocation, "Story.Event.Move.ToLocation");

FHktDefaultClientRule::FHktDefaultClientRule()
{
}

FHktDefaultClientRule::~FHktDefaultClientRule()
{
}

// ============================================================================
// 컨텍스트 바인딩
// ============================================================================

void FHktDefaultClientRule::BindContext(
	IHktProxySimulator*      InSimulator,
	IHktIntentBuilder*       InBuilder,
	IHktUnitSelectionPolicy* InPolicy,
	IHktCommandContainer*    InContainer,
	IHktWorldPlayer*         InWorldPlayer)
{
	CachedSimulator   = InSimulator;
	CachedBuilder     = InBuilder;
	CachedPolicy      = InPolicy;
	CachedContainer   = InContainer;
	CachedWorldPlayer = InWorldPlayer;
}

// ============================================================================
// 소유권 체크
// ============================================================================

bool FHktDefaultClientRule::IsOwnedByMe(FHktEntityId Entity) const
{
	if (!CachedWorldPlayer || !CachedSimulator || !CachedSimulator->IsInitialized())
		return false;

	const int64 MyUid = CachedWorldPlayer->GetPlayerUid();
	if (MyUid == 0) return false;

	const FHktWorldState& WS = CachedSimulator->GetWorldState();
	return WS.GetOwnerUid(Entity) == MyUid;
}

// ============================================================================
// 기본 액션 결정 (슬롯 미선택 시)
// ============================================================================

FHktEvent FHktDefaultClientRule::BuildDefaultAction(FHktEntityId TargetEntity, FVector TargetLocation) const
{
	FHktEvent Event;
	Event.TargetEntity = TargetEntity;
	Event.Location = TargetLocation;

	if (TargetEntity != InvalidEntityId && CachedSimulator && CachedSimulator->IsInitialized())
	{
		const FHktWorldState& WS = CachedSimulator->GetWorldState();
		if (WS.IsValidEntity(TargetEntity))
		{
			// 바닥 아이템 → Pickup
			const int32 ItemId = WS.GetProperty(TargetEntity, PropertyId::ItemId);
			const int32 ItemState = WS.GetProperty(TargetEntity, PropertyId::ItemState);
			if (ItemId > 0 && ItemState == 0)
			{
				Event.EventTag = Tag_Event_Item_Pickup;
				return Event;
			}

			// NPC/캐릭터 → 기본 공격
			const int32 IsNPC = WS.GetProperty(TargetEntity, PropertyId::IsNPC);
			if (IsNPC > 0)
			{
				Event.EventTag = Tag_Event_Attack_Basic;
				return Event;
			}
		}
	}

	// 대상 없거나 특수 타입 아님 → 이동
	Event.EventTag = Tag_Event_Move_ToLocation;
	return Event;
}

// ============================================================================
// 유저 이벤트
// ============================================================================

void FHktDefaultClientRule::OnUserEvent_LoginButtonClick() {}

void FHktDefaultClientRule::OnUserEvent_SubjectInputAction()
{
	if (!CachedPolicy || !CachedBuilder) return;

	// 아무 엔티티나 Subject로 선택 (아이템 포함, 하드코딩 제거)
	FHktEntityId SelectedEntity = CachedPolicy->ResolveSubject();
	CachedBuilder->SetSubject(SelectedEntity);
	CachedBuilder->ResetCommand();
}

void FHktDefaultClientRule::OnUserEvent_TargetInputAction()
{
	if (!CachedPolicy || !CachedBuilder) return;

	// Subject 없으면 무반응
	FHktEntityId SubjectEntity = CachedBuilder->GetSubjectEntityId();
	if (SubjectEntity == InvalidEntityId) return;

	// 내 소유 아닌 Subject면 관찰만 (제어 불가)
	if (!IsOwnedByMe(SubjectEntity)) return;

	// Target 해석
	FHktEntityId TargetEntity = InvalidEntityId;
	FVector TargetLocation = FVector::ZeroVector;
	CachedPolicy->ResolveTarget(TargetEntity, TargetLocation);
	CachedBuilder->SetTarget(TargetEntity, TargetLocation);

	const int32 PendingSlot = CachedBuilder->GetCommandSlotIndex();

	FHktEvent Event;
	if (PendingSlot >= 0 && CachedContainer)
	{
		// SlotAction 선택됨 → 해당 슬롯의 EventTag로 이벤트 생성
		FGameplayTag EventTag = CachedContainer->GetEventTagAtSlot(PendingSlot);
		Event.EventTag = EventTag;
		Event.SourceEntity = SubjectEntity;
		Event.TargetEntity = TargetEntity;
		Event.Location = TargetLocation;
		Event.Param0 = PendingSlot;
	}
	else
	{
		// SlotAction 없음 → 기본 액션 (타겟 유형 기반)
		Event = BuildDefaultAction(TargetEntity, TargetLocation);
		Event.SourceEntity = SubjectEntity;
	}

	CachedBuilder->SetPendingRuntimeEvent(Event);
	CachedBuilder->ResetCommand();

	HKT_EVENT_LOG_TAG(HktLogTags::Runtime_Intent, EHktLogLevel::Info, EHktLogSource::Client,
		FString::Printf(TEXT("TargetAction %s"), *Event.ToString()),
		SubjectEntity, Event.EventTag);
}

void FHktDefaultClientRule::OnUserEvent_CommandInputAction(int32 InSlotIndex)
{
	if (!CachedBuilder) return;

	// Subject 없거나 제어 불가능하면 무시
	FHktEntityId SubjectEntity = CachedBuilder->GetSubjectEntityId();
	if (SubjectEntity == InvalidEntityId) return;
	if (!IsOwnedByMe(SubjectEntity)) return;

	CachedBuilder->SetCommandSlot(InSlotIndex);
}

void FHktDefaultClientRule::OnUserEvent_ZoomInputAction(float InDelta)
{
}

// ============================================================================
// 수신 이벤트 (내부 캐싱된 Simulator 사용)
// ============================================================================

void FHktDefaultClientRule::OnReceived_InitialState(const FHktWorldState& InState, int32 InGroupIndex)
{
	if (!CachedSimulator) return;

	CachedSimulator->RestoreState(InState, InGroupIndex);

	// 초기화 전 도착한 서버 Batch를 큐에 적재 (틱에서 처리)
	for (const FHktSimulationEvent& B : PendingBatches)
	{
		if (B.FrameNumber > InState.FrameNumber)
		{
			CachedSimulator->EnqueueServerBatch(B);
		}
	}
	PendingBatches.Empty();
}

void FHktDefaultClientRule::OnReceived_FrameBatch(const FHktSimulationEvent& InBatch)
{
	if (!CachedSimulator || !CachedSimulator->IsInitialized())
	{
		PendingBatches.Add(InBatch);
		return;
	}
	// 즉시 처리하지 않고 큐에 적재 — 다음 틱에서 롤백/빨리감기 처리
	CachedSimulator->EnqueueServerBatch(InBatch);
}

void FHktDefaultClientRule::OnReceived_BagUpdate(const FHktBagDelta& InDelta)
{
	// BagComponent가 Client_ReceiveBagUpdate에서 직접 LocalBagState를 업데이트하고
	// 델리게이트를 브로드캐스트하므로, ClientRule에서는 추가 처리 불필요.
	// 향후 UI 시스템이 복잡해지면 여기서 추가 로직 가능.
}
