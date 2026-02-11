// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktRuleInterfaces.h"
#include "IHktSimulationWorld.h"
#include "HktClientSimulatorComponent.generated.h"

UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktClientSimulatorComponent : public UActorComponent, public IHktSimulator
{
    GENERATED_BODY()

public:
    UHktClientSimulatorComponent();

    virtual void Execute(const FHktRuntimeBatch& InBatch) override;
    virtual void RestoreState(const FHktRuntimeSimulationState& InState, TArray<FHktRuntimeBatch>&& InPendingBatches) override;
    virtual const FHktRuntimeSimulationState& GetSimulationState() const override { return State; }
    virtual FHktRuntimeOwnerState GetOwnerState(int64 InOwnerId) const override;
    virtual bool IsInitialized() const override { return bInitialized; }

protected:
    virtual void BeginPlay() override;

private:
    /** HktCore 시뮬레이션 월드 (결정론적 시뮬레이션 위임 대상) */
    TUniquePtr<IHktSimulationWorld> SimWorld;

    FHktRuntimeSimulationState State;
    bool bInitialized = false;
};
