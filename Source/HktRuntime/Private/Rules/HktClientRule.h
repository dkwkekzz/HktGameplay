#pragma once

#include "CoreMinimal.h"
#include "HktRuleInterfaces.h"
#include "HktRuntimeTypes.h"

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

class IHktSubjectSelectionPolicy
{
public:
    virtual ~IHktSubjectSelectionPolicy() = default;
    virtual FHktEntityId ResolveSubject() const = 0;
};

class IHktTargetSelectionPolicy
{
public:
    virtual ~IHktTargetSelectionPolicy() = default;
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
    virtual void OnUserEvent_SubjectInputAction(const IHktSubjectSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_TargetInputAction(const IHktTargetSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) override;
    virtual void OnReceived_InitialSimulationState(const FHktGroupSimulationState& InState, IHktSimulator& InSimulator) override;
    virtual void OnReceived_FrameBatch(const FHktFrameBatch& InBatch, IHktSimulator& InSimulator) override;

private:
    TArray<FHktFrameBatch> PendingFrameBatches;
};
