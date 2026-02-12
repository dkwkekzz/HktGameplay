// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/HktServerRule.h"
#include "HktSimulator.h"

class HKTRUNTIME_API FHktServerSimulator : public IHktServerSimulator
{
public:
    FHktServerSimulator();
    virtual ~FHktServerSimulator() = default;

    virtual void Execute(const FHktSimulationEvent& InBatch) override;
    virtual const FHktRuntimeSimulationState& GetSimulationState() const override { return State; }
    virtual FHktRuntimeOwnerState GetOwnerState(int64 InOwnerId) const override;

private:
    /** HktCore 시뮬레이션 월드 (결정론적 시뮬레이션 위임 대상) */
    TUniquePtr<IHktSimulator> CoreSimulator;

    FHktRuntimeSimulationState State;
    bool bInitialized = false;
};
