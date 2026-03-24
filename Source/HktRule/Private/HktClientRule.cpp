// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClientRule.h"
#include "HktCoreProperties.h"

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
	IHktCommandContainer*    InContainer)
{
	CachedSimulator = InSimulator;
	CachedBuilder   = InBuilder;
	CachedPolicy    = InPolicy;
	CachedContainer = InContainer;
}

// ============================================================================
// 유저 이벤트 (내부 캐싱된 컨텍스트 사용)
// ============================================================================

void FHktDefaultClientRule::OnUserEvent_LoginButtonClick() {}

void FHktDefaultClientRule::OnUserEvent_SubjectInputAction()
{
	if (!CachedPolicy || !CachedBuilder) return;

	FHktEntityId SelectedEntity = CachedPolicy->ResolveSubject();
	if (SelectedEntity == InvalidEntityId) return;

	// 바닥 아이템 판단 → Subject 선택이 아닌 Pickup 처리
	if (CachedSimulator && CachedSimulator->IsInitialized())
	{
		const FHktWorldState& WS = CachedSimulator->GetWorldState();
		if (WS.IsValidEntity(SelectedEntity))
		{
			const int32 ItemId = WS.GetProperty(SelectedEntity, PropertyId::ItemId);
			const int32 ItemState = WS.GetProperty(SelectedEntity, PropertyId::ItemState);
			if (ItemId > 0 && ItemState == 0)
			{
				CachedBuilder->SetPendingItemPickup(SelectedEntity);
				return;
			}
		}
	}

	CachedBuilder->SetSubject(SelectedEntity);
	CachedBuilder->ResetCommand();
}

void FHktDefaultClientRule::OnUserEvent_TargetInputAction()
{
	if (!CachedPolicy || !CachedBuilder) return;

	FHktEntityId TargetEntity = InvalidEntityId;
	FVector TargetLocation    = FVector::ZeroVector;
	CachedPolicy->ResolveTarget(TargetEntity, TargetLocation);
	CachedBuilder->SetTarget(TargetEntity, TargetLocation);
	// Submit은 하지 않음 — PlayerController::OnTargetAction에서 요청 패킷 직접 전송
}

void FHktDefaultClientRule::OnUserEvent_CommandInputAction(int32 InSlotIndex)
{
	// 클라이언트는 EventTag를 해석하지 않음 — 슬롯 인덱스만 기록
	// 실제 요청 패킷 전송은 PlayerController::OnSlotAction에서 처리
	if (!CachedBuilder) return;
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
