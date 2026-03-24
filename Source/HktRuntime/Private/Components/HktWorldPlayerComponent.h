// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktServerRuleInterfaces.h"
#include "HktWorldPlayerComponent.generated.h"

class AHktInGamePlayerController;
class APlayerController;
class APlayerState;
class UHktBagComponent;

UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktWorldPlayerComponent : public UActorComponent, public IHktWorldPlayer
{
    GENERATED_BODY()

public:
    UHktWorldPlayerComponent();

    virtual int64 GetPlayerUid() const override;
    virtual AActor* GetOwnerActor() const override { return GetOwner(); }
    virtual bool IsInitialized() const override;
    virtual void InvalidatePlayerUidCache() override;
    virtual IHktPlayerBag* GetPlayerBag() const override;

protected:
    virtual void BeginPlay() override;

private:
    void UpdatePlayerUidFromPlayerState() const;

    mutable int64 PlayerUid = 0;
    mutable bool bPlayerUidCached = false;
    mutable TWeakObjectPtr<UActorComponent> CachedBagComponent;
};
