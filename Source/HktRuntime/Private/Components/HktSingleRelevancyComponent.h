// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktServerRuleInterfaces.h"
#include "HktSimulator.h"
#include "HktRuntimeTypes.h"
#include "HktSingleRelevancyComponent.generated.h"

UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktSingleRelevancyComponent : public UActorComponent, public IHktRelevancyGraph, public IHktRelevancyGroup
{
    GENERATED_BODY()

public:
    UHktSingleRelevancyComponent();

    // IHktRelevancyGraph
    virtual void RegisterPlayer(IHktWorldPlayer* Player, int32 GroupIndex) override;
    virtual void UnregisterPlayer(int64 PlayerUid) override;
    virtual void UpdateRelevancy() override;
    virtual IHktWorldPlayer* GetWorldPlayer(int64 PlayerUid) const override;
    virtual int32 GetWorldPlayerCount() const override { return RegisteredPlayers.Num(); }
    virtual int32 NumRelevancyGroup() const override;
    virtual IHktRelevancyGroup& GetRelevancyGroup(int32 Index) override;
    virtual const IHktRelevancyGroup& GetRelevancyGroup(int32 Index) const override;
    virtual IHktRelevancyGroup* GetRelevancyGroupByPlayer(int64 PlayerUid) override;
    virtual const IHktRelevancyGroup* GetRelevancyGroupByPlayer(int64 PlayerUid) const override;
    virtual int32 GetRelevancyGroupIndex(int64 PlayerUid) const override;

    // IHktRelevancyGroup
    virtual IHktAuthoritySimulator& GetSimulator() override { return *Simulator; }
    virtual const IHktAuthoritySimulator& GetSimulator() const override { return *Simulator; }
    virtual const TArray<int64>& GetPlayerUids() const override { return PlayerUids; }
    virtual const TArray<IHktWorldPlayer*>& GetCachedWorldPlayers() const override { return CachedPlayers; }

protected:
    virtual void BeginPlay() override;

private:
    TMap<int64, IHktWorldPlayer*> RegisteredPlayers;
    TUniquePtr<IHktAuthoritySimulator> Simulator;
    TArray<int64> PlayerUids;
    TArray<IHktWorldPlayer*> CachedPlayers;
};
