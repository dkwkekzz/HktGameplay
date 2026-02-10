// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktRuleInterfaces.h"

class HKTRUNTIME_API FHktServerSimulator : public IHktSimulator
{
public:
    FHktServerSimulator() = default;
    virtual ~FHktServerSimulator() = default;

    virtual void Execute(const FHktFrameBatch& InBatch) override;
    virtual void RestoreState(const FHktGroupSimulationState& InState, TArray<FHktFrameBatch>&& InPendingBatches) override;
    virtual const FHktGroupSimulationState& GetSimulationState() const override { return State; }
    virtual FHktOwnerSimulationState GetOwnerSimulationState(int64 InOwnerId) const override;
    virtual bool IsInitialized() const override { return bInitialized; }

private:
    FHktGroupSimulationState State;
    bool bInitialized = false;
};
