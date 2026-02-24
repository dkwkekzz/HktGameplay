// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktClientRuleInterfaces.h"
#include "HktSimulator.h"

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

    // === IHktClientSimulator ===
    virtual void RestoreState(const FHktWorldState& InState) override;
    virtual const FHktWorldState& GetWorldState() const override;
    virtual bool IsInitialized() const override;
    virtual void AdvanceLocalFrame(float DeltaSeconds) override;
    virtual FHktSimulationDiff ReconcileWithServerBatch(const FHktSimulationEvent& InBatch) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_HKT_INSIGHTS
public:
	virtual void CollectInsightData(FHktInsightSnapshot& OutSnapshot) const override;
	virtual FString GetInsightProviderName() const override { return TEXT("ProxySimulator"); }
#endif

private:
    /** ?? Batch ???? (???? ???) */
    FHktSimulationEvent BuildLocalBatch(int64 Frame, float DeltaSeconds) const;

    /** ??? ?? ?????? ????? */
    void ReplayFromSnapshot();

    // --- ?ù?????? ---
    TUniquePtr<IHktAuthoritySimulator> Simulator;
    FHktSchemaRegistry SchemaRegistry;
    bool bInitialized = false;

    // --- ?????? (?????? ???? ??? ????) ---
    FHktWorldState SnapshotState;
    int64 SnapshotFrame = 0;

    // --- ???? ?????? (?????? ???? ?????? Batch??) ---
    TArray<FHktSimulationEvent> LocalHistory;

    // --- ???? ?????? ---
    float FrameAccumulator = 0.0f;
    int64 LocalFrame = 0;

    static constexpr float FixedDeltaTime = 1.0f / 30.0f;

    // --- ?????? ???? (??? ???) ---
    static constexpr int32 MaxHistoryFrames = 300; // 10?? @ 30Hz
};
