// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktProxySimulatorComponent.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#include "HktWorldStateInsightsHelper.h"
#endif

UHktProxySimulatorComponent::UHktProxySimulatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHktProxySimulatorComponent::BeginPlay()
{
	Super::BeginPlay();
	SchemaRegistry.Initialize();
	State.Initialize(SchemaRegistry);

	HKT_INSIGHTS_REGISTER_PROVIDER(this);
}

void UHktProxySimulatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HKT_INSIGHTS_UNREGISTER_PROVIDER(this);
	Super::EndPlay(EndPlayReason);
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

// ============================================================================
// IHktInsightProvider 구현
// ============================================================================

#if WITH_HKT_INSIGHTS
void UHktProxySimulatorComponent::CollectInsightData(FHktInsightSnapshot& OutSnapshot) const
{
	OutSnapshot.ProviderName = TEXT("ProxySimulator");

	const FString Cat = TEXT("Client WorldState");
	OutSnapshot.AddInfo(Cat, TEXT("Initialized"), bInitialized ? TEXT("Yes") : TEXT("No"));
	OutSnapshot.AddInfo(Cat, TEXT("Frame"),    FString::Printf(TEXT("%lld"), State.FrameNumber));
	OutSnapshot.AddInfo(Cat, TEXT("Entities"), FString::FromInt(State.GetEntityCount()));

	// WorldState 전용 패널용 스냅샷 push
	if (bInitialized && State.GetEntityCount() > 0)
	{
		FHktWorldStateSnapshot Snapshot = HktWorldStateInsights::BuildSnapshot(State, TEXT("Client"));
		FHktRuntimeInsightsCollector::Get().PushWorldStateSnapshot(MoveTemp(Snapshot));
	}
}
#endif
