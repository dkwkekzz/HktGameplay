// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktProxySimulatorComponent.h"

UHktProxySimulatorComponent::UHktProxySimulatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHktProxySimulatorComponent::BeginPlay()
{
	Super::BeginPlay();
	SchemaRegistry.Initialize();
	State.Initialize(SchemaRegistry);
}

void UHktProxySimulatorComponent::ApplyDiff(const FHktSimulationDiff& InDiff)
{
	for (FHktEntityId Id : InDiff.RemovedEntities)
		State.RemoveEntity(Id);

	for (const FHktEntityState& ES : InDiff.SpawnedEntities)
		State.ImportEntityState(ES);

	for (const FHktPropertyDelta& D : InDiff.PropertyDeltas)
		State.SetPropertyDirty(D.EntityId, D.PropertyId, D.NewValue);

	State.FrameNumber = InDiff.FrameNumber;
}

void UHktProxySimulatorComponent::RestoreState(const FHktWorldState& InState)
{
	State.CopyFrom(InState);
	State.Registry = &SchemaRegistry;
	for (int32 T = 1; T < HktType::MaxTypes; ++T)
		State.Pools[T].Schema = &SchemaRegistry.Get(static_cast<FHktTypeId>(T));

	bInitialized = true;
}
