// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClientRule.h"

FHktDefaultClientRule::FHktDefaultClientRule()
{
}

FHktDefaultClientRule::~FHktDefaultClientRule()
{
}

// ============================================================================
// 유저 입력
// ============================================================================

void FHktDefaultClientRule::OnUserEvent_LoginButtonClick()
{
    // 기본 구현: no-op
    // 오케스트레이터(EntryPlayerController)가 RequestLogin RPC를 직접 호출.
    // Rule에서는 입력 검증, 중복 클릭 방지 등의 정책만 처리.
}

void FHktDefaultClientRule::OnUserEvent_SubjectInputAction(
    const IHktSubjectSelectionPolicy& InPolicy,
    IHktIntentBuilder& InBuilder)
{
    FHktEntityId SelectedEntity = InPolicy.ResolveSubject();
    if (SelectedEntity == InvalidEntityId)
    {
        return;
    }

    InBuilder.SetSubject(SelectedEntity);
    InBuilder.ResetCommand();
}

void FHktDefaultClientRule::OnUserEvent_TargetInputAction(
    const IHktTargetSelectionPolicy& InPolicy,
    IHktIntentBuilder& InBuilder)
{
    FHktEntityId TargetEntity = InvalidEntityId;
    FVector TargetLocation = FVector::ZeroVector;
    InPolicy.ResolveTarget(TargetEntity, TargetLocation);

    InBuilder.SetTarget(TargetEntity, TargetLocation);

    if (InBuilder.IsReadyToSubmit())
    {
        InBuilder.Submit();
    }
}

void FHktDefaultClientRule::OnUserEvent_CommandInputAction(
    const IHktCommandContainer& InContainer,
    int32 InSlotIndex,
    IHktIntentBuilder& InBuilder)
{
    FGameplayTag EventTag = InContainer.GetEventTagAtSlot(InSlotIndex);
    if (!EventTag.IsValid())
    {
        return;
    }

    bool bTargetRequired = InContainer.IsTargetRequiredAtSlot(InSlotIndex);
    InBuilder.SetCommand(EventTag, bTargetRequired);

    if (!bTargetRequired && InBuilder.IsReadyToSubmit())
    {
        InBuilder.Submit();
    }
}

void FHktDefaultClientRule::OnUserEvent_ZoomInputAction(float InDelta)
{
    // 기본 구현: no-op. 카메라/Presentation 레이어에서 처리.
}

// ============================================================================
// 서버 수신
// ============================================================================

void FHktDefaultClientRule::OnReceived_InitialSimulationState(
    const FHktGroupSimulationState& InState,
    IHktSimulator& InSimulator)
{
    // [Newbie Path]
    // 서버가 시뮬레이션을 완료한 "직후"의 그룹 전체 상태를 통째로 받는다.
    // 클라이언트는 이 상태를 복원하여 서버와 동기화된 시점에서 시작.
    //
    // 서버 흐름:
    //   OnTick_ExecuteFrame → GroupSimulator.Execute(Batch)
    //   → Group.GetCurrentSimulationState() 복사
    //   → OnTick_SendFrameBatch → Player->Client_ReceiveInitialState(State)
    //
    // 복원 후 다음 프레임부터는 OnReceived_FrameBatch로 전환됨.
    // (전환 시점 관리는 오케스트레이터(PlayerController)가 담당)

    InSimulator.RestoreState(InState, MoveTemp(PendingFrameBatches));
    PendingFrameBatches.Empty();

    UE_LOG(LogTemp, Log, TEXT("[ClientRule] InitialState received: Frame=%lld, Entities=%d, ActiveEvents=%d"),
        InState.LastProcessedFrameNumber,
        InState.EntitySnapshots.Num(),
        InState.ActiveEvents.Num());
}

void FHktDefaultClientRule::OnReceived_FrameBatch(
    const FHktFrameBatch& InBatch,
    IHktSimulator& InSimulator)
{
    // [Existing Path]
    // 서버와 동일한 입력(FrameBatch)을 받아 결정론적 시뮬레이션을 로컬에서 수행.
    // 서버와 클라이언트가 같은 입력 + 같은 로직 + 같은 RandomSeed로
    // 동일한 결과를 독립적으로 계산 → 결과 동기화 보장.
    //
    // FrameBatch 구성:
    //   - FrameNumber: 프레임 번호
    //   - RandomSeed:  결정론적 난수 시드
    //   - DeltaSeconds: 프레임 델타
    //   - Events:       이번 프레임의 Intent들
    //   - RemovedOwnerIds: 로그아웃한 플레이어의 엔티티 제거

    if (!InSimulator.IsInitialized())
    {
        // InitialState를 아직 받지 못한 상태에서 FrameBatch가 도착.
		PendingFrameBatches.Add(InBatch);

        UE_LOG(LogTemp, Warning, TEXT("[ClientRule] FrameBatch received before InitialState. Ignoring Frame=%lld"),
            InBatch.FrameNumber);
    }
	else
	{
		InSimulator.Execute(InBatch);
	}
}