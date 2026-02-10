// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktServerRule.h"
#include "HktWorldPlayerComponent.generated.h"

class AHktInGamePlayerController;

UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktWorldPlayerComponent : public UActorComponent, public IHktWorldPlayer
{
    GENERATED_BODY()

public:
    UHktWorldPlayerComponent();

    virtual int64 GetPlayerUid() const override { return PlayerUid; }

    virtual void SendFrameBatch(const FHktFrameBatch& Batch) override;
    virtual void SendInitialSimulationState(const FHktGroupSimulationState& InitialState) override;

    void SetPlayerUid(int64 InUid) { PlayerUid = InUid; }
    bool IsInitialized() const { return PlayerUid != 0; }

private:
    AHktInGamePlayerController* GetOwnerPC() const;

    int64 PlayerUid = 0;
};
