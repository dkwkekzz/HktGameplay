// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktClientRuleInterfaces.h"
#include "HktProxySimulatorComponent.generated.h"

UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktProxySimulatorComponent : public UActorComponent, public IHktProxySimulator
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

private:
	FHktWorldState State;
	FHktSchemaRegistry SchemaRegistry;
	bool bInitialized = false;
};
