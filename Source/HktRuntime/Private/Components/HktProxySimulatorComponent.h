// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktClientRuleInterfaces.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightProvider.h"
#endif

#include "HktProxySimulatorComponent.generated.h"

UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktProxySimulatorComponent : public UActorComponent, public IHktProxySimulator, public IHktInsightProvider
{
	GENERATED_BODY()

public:
	UHktProxySimulatorComponent();

	virtual void ApplyDiff(const FHktSimulationDiff& InDiff) override;
	virtual void RestoreState(const FHktWorldState& InState) override;
	virtual const FHktWorldState& GetWorldState() const override { return State; }
	virtual bool IsInitialized() const override { return bInitialized; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_HKT_INSIGHTS
public:
	virtual void CollectInsightData(FHktInsightSnapshot& OutSnapshot) const override;
	virtual FString GetInsightProviderName() const override { return TEXT("ProxySimulator"); }
#endif

private:
	FHktWorldState State;
	FHktSchemaRegistry SchemaRegistry;
	bool bInitialized = false;
};
