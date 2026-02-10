// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktRuleInterfaces.h"
#include "HktClientSimulatorComponent.generated.h"

UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktClientSimulatorComponent : public UActorComponent, public IHktSimulator
{
    GENERATED_BODY()

public:
    UHktClientSimulatorComponent();

    virtual void Execute(const FHktFrameBatch& InBatch) override;
    virtual void RestoreState(const FHktGroupSimulationState& InState, TArray<FHktFrameBatch>&& InPendingBatches) override;
    virtual const FHktGroupSimulationState& GetSimulationState() const override { return State; }
    virtual FHktOwnerSimulationState GetOwnerSimulationState(int64 InOwnerId) const override;
    virtual bool IsInitialized() const override { return bInitialized; }

protected:
    virtual void BeginPlay() override;

private:
    FHktGroupSimulationState State;
    bool bInitialized = false;
};
