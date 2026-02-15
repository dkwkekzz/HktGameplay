// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktClientRule.h"
#include "HktSimulator.h"
#include "HktClientSimulatorComponent.generated.h"

UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktClientSimulatorComponent : public UActorComponent, public IHktClientSimulator
{
    GENERATED_BODY()

public:
    UHktClientSimulatorComponent();

    virtual void Execute(const FHktSimulationEvent& InBatch) override;
    virtual void RestoreState(const FHktWorldState& InState, TArray<FHktSimulationEvent>&& InPendingBatches) override;
    virtual const FHktWorldState& GetSimulationState() const override { return State; }
    virtual bool IsInitialized() const override { return bInitialized; }

protected:
    virtual void BeginPlay() override;

private:
    /** HktCore 시뮬레이션 월드 (결정론적 시뮬레이션 위임 대상) */
    TUniquePtr<IHktSimulator> CoreSimulator;

    FHktWorldState State;
    bool bInitialized = false;
};
