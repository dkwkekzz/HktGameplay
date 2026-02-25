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
	if (!bTargetRequired && CachedBuilder->IsReadyToSubmit()) { CachedBuilder->Submit(); }
}

void FHktDefaultClientRule::OnUserEvent_ZoomInputAction(float InDelta)
{
}

// ============================================================================
// 수신 이벤트 (내부 캐싱된 Simulator 사용)
// ============================================================================

void FHktDefaultClientRule::OnReceived_InitialState(const FHktWorldState& InState)
{
	if (!CachedSimulator) return;

	CachedSimulator->RestoreState(InState);

	for (const FHktSimulationEvent& B : PendingBatches)
	{
		if (B.FrameNumber > InState.FrameNumber)
		{
			CachedSimulator->ReconcileWithServerBatch(B);
		}
	}
	PendingBatches.Empty();
}

FHktSimulationDiff FHktDefaultClientRule::OnReceived_FrameBatch(const FHktSimulationEvent& InBatch)
{
	if (!CachedSimulator)
	{
		PendingBatches.Add(InBatch);
		return FHktSimulationDiff();
	}
	if (!CachedSimulator->IsInitialized())
	{
		PendingBatches.Add(InBatch);
		return FHktSimulationDiff();
	}
	return CachedSimulator->ReconcileWithServerBatch(InBatch);
}
