// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktRuleInterfaces.h"
#include "IHktSimulationWorld.h"

class HKTRUNTIME_API FHktServerSimulator : public IHktSimulator
{
public:
    FHktServerSimulator();
    virtual ~FHktServerSimulator() = default;

    virtual void Execute(const FHktRuntimeBatch& InBatch) override;
    virtual void RestoreState(const FHktRuntimeSimulationState& InState, TArray<FHktRuntimeBatch>&& InPendingBatches) override;
    virtual const FHktRuntimeSimulationState& GetSimulationState() const override { return State; }
    virtual FHktRuntimeOwnerState GetOwnerState(int64 InOwnerId) const override;
    virtual bool IsInitialized() const override { return bInitialized; }

private:
    /** HktCore 시뮬레이션 월드 (결정론적 시뮬레이션 위임 대상) */
    TUniquePtr<IHktSimulationWorld> SimWorld;

    FHktRuntimeSimulationState State;
    bool bInitialized = false;
};
