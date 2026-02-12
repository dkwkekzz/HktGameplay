// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktServerRule.h"
#include "HktBatchBuilderComponent.generated.h"

UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktBatchBuilderComponent : public UActorComponent, public IHktBatchBuilder
{
    GENERATED_BODY()

public:
    UHktBatchBuilderComponent();

    virtual FHktSimulationEvent& CreateOrGetGroupFrameBatch(int32 InGroupIdx) override;
    virtual const FHktSimulationEvent& GetGroupFrameBatch(int32 InGroupIdx) const override;
    virtual TArray<int64>& GetMutableNewbieOwners(int32 InGroupIdx) override;
    virtual const TArray<int64>& GetNewbieOwners(int32 InGroupIdx) const override;

    void Reset(int32 NumGroups);

private:
    TArray<FHktSimulationEvent> GroupBatches;
    TArray<TArray<int64>> NewbieOwners;
    static const TArray<int64> EmptyOwners;
};
