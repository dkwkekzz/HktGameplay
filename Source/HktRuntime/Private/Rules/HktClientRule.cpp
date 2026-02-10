// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClientRule.h"

FHktDefaultClientRule::FHktDefaultClientRule() {}
FHktDefaultClientRule::~FHktDefaultClientRule() {}

void FHktDefaultClientRule::OnUserEvent_LoginButtonClick() {}

void FHktDefaultClientRule::OnUserEvent_SubjectInputAction(const IHktSubjectSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder)
{
    FHktEntityId SelectedEntity = InPolicy.ResolveSubject();
    if (SelectedEntity == InvalidEntityId) return;
    InBuilder.SetSubject(SelectedEntity);
    InBuilder.ResetCommand();
}

void FHktDefaultClientRule::OnUserEvent_TargetInputAction(const IHktTargetSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder)
{
    FHktEntityId TargetEntity = InvalidEntityId;
    FVector TargetLocation = FVector::ZeroVector;
    InPolicy.ResolveTarget(TargetEntity, TargetLocation);
    InBuilder.SetTarget(TargetEntity, TargetLocation);
    if (InBuilder.IsReadyToSubmit()) { InBuilder.Submit(); }
}

void FHktDefaultClientRule::OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder)
{
    FGameplayTag EventTag = InContainer.GetEventTagAtSlot(InSlotIndex);
    if (!EventTag.IsValid()) return;
    bool bTargetRequired = InContainer.IsTargetRequiredAtSlot(InSlotIndex);
    InBuilder.SetCommand(EventTag, bTargetRequired);
    if (!bTargetRequired && InBuilder.IsReadyToSubmit()) { InBuilder.Submit(); }
}

void FHktDefaultClientRule::OnUserEvent_ZoomInputAction(float InDelta) {}

void FHktDefaultClientRule::OnReceived_InitialSimulationState(const FHktGroupSimulationState& InState, IHktSimulator& InSimulator)
{
    InSimulator.RestoreState(InState, MoveTemp(PendingFrameBatches));
    PendingFrameBatches.Empty();
}

void FHktDefaultClientRule::OnReceived_FrameBatch(const FHktFrameBatch& InBatch, IHktSimulator& InSimulator)
{
    if (!InSimulator.IsInitialized()) { PendingFrameBatches.Add(InBatch); return; }
    InSimulator.Execute(InBatch);
}
