// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClientRule.h"

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
	if (CachedBuilder->IsReadyToSubmit()) { CachedBuilder->Submit(); }
}

void FHktDefaultClientRule::OnUserEvent_CommandInputAction(int32 InSlotIndex)
{
	if (!CachedContainer || !CachedBuilder) return;

	FGameplayTag EventTag = CachedContainer->GetEventTagAtSlot(InSlotIndex);
	if (!EventTag.IsValid()) return;
	bool bTargetRequired = CachedContainer->IsTargetRequiredAtSlot(InSlotIndex);
	CachedBuilder->SetCommand(EventTag, bTargetRequired);
	CachedBuilder->SetCommandSlot(InSlotIndex);
	if (!bTargetRequired && CachedBuilder->IsReadyToSubmit()) { CachedBuilder->Submit(); }
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
