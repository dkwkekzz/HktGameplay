#pragma once

#include "CoreMinimal.h"
#include "HktRuntimeTypes.h"
#include "HktClientRuleInterfaces.h"

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
    virtual void OnReceived_InitialState(const FHktWorldState& InState, IHktProxySimulator& InSimulator) override;
    virtual FHktSimulationDiff OnReceived_FrameBatch(const FHktSimulationEvent& InBatch, IHktProxySimulator& InSimulator) override;

private:
    TArray<FHktSimulationEvent> PendingBatches;
};
