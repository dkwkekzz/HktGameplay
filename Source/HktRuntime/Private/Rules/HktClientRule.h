#pragma once

#include "CoreMinimal.h"
#include "HktRuleInterfaces.h"
#include "HktRuntimeTypes.h"

// ============================================================================
// IHktClientSimulator - ???????? ???????
//
// ???????: PlayerController?? ??? ???? (ClientSimulatorComponent)
//
// ???? ??:
//   Execute(Batch)    : FrameBatch(???)?? ?? ?????? ??????
//   RestoreState(State): ?????? ????? ???? ??? ???? (??? ????)
//   GetSimulationState(): ???? ?????? ???? ???
// ============================================================================
class IHktClientSimulator
{
public:
    virtual ~IHktClientSimulator() = default;

    /** FrameBatch(???)?? ?? ?????? ?????? */
    virtual void Execute(const FHktSimulationEvent& InBatch) = 0;

    /** ?????? ????? ???? ??? ???? + ??? ????? ??? ??? */
    virtual void RestoreState(const FHktWorldState& InState, TArray<FHktSimulationEvent>&& InPendingBatches) = 0;

    /** 현재 시뮬레이션 상태 조회 (Newbie 전송, 저장 등에 사용) */
    virtual const FHktWorldState& GetSimulationState() const = 0;

    /** ???? ??? ???? (RestoreState ??? ?Execute ???? true) */
    virtual bool IsInitialized() const = 0;
};

class IHktIntentBuilder
{
public:
    virtual ~IHktIntentBuilder() = default;
    virtual void SetSubject(FHktEntityId InSubject) = 0;
    virtual void SetCommand(FGameplayTag InEventTag, bool bInTargetRequired) = 0;
    virtual void SetTarget(FHktEntityId InTarget, FVector InLocation) = 0;
    virtual void ResetCommand() = 0;
    virtual bool IsReadyToSubmit() const = 0;
    virtual bool Submit() = 0;
};

class IHktUnitSelectionPolicy
{
public:
    virtual ~IHktUnitSelectionPolicy() = default;
    virtual FHktEntityId ResolveSubject() const = 0;
    virtual void ResolveTarget(FHktEntityId& OutEntity, FVector& OutLocation) const = 0;
};

class IHktCommandContainer
{
public:
    virtual ~IHktCommandContainer() = default;
    virtual FGameplayTag GetEventTagAtSlot(int32 SlotIndex) const = 0;
    virtual bool IsTargetRequiredAtSlot(int32 SlotIndex) const = 0;
    virtual int32 GetNumSlots() const = 0;
};

class HKTRUNTIME_API FHktDefaultClientRule : public IHktClientRule
{
public:
    FHktDefaultClientRule();
    virtual ~FHktDefaultClientRule();

    virtual void OnUserEvent_LoginButtonClick() override;
    virtual void OnUserEvent_SubjectInputAction(const IHktUnitSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_TargetInputAction(const IHktUnitSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) override;
    virtual void OnReceived_InitialSimulationState(const FHktWorldState& InState, IHktClientSimulator& InSimulator) override;
    virtual void OnReceived_FrameBatch(const FHktSimulationEvent& InBatch, IHktClientSimulator& InSimulator) override;

private:
    TArray<FHktSimulationEvent> PendingFrameBatches;
};
