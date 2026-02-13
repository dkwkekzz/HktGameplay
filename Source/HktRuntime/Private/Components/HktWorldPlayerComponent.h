// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktServerRule.h"
#include "HktWorldPlayerComponent.generated.h"

class AHktInGamePlayerController;
class APlayerController;
class APlayerState;

UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktWorldPlayerComponent : public UActorComponent, public IHktWorldPlayer
{
    GENERATED_BODY()

public:
    UHktWorldPlayerComponent();

    virtual int64 GetPlayerUid() const override;
    virtual AActor* GetOwnerActor() const override { return GetOwner(); }

    /** PlayerState 변경 시 캐시를 무효화합니다. */
    void InvalidatePlayerUidCache();

    bool IsInitialized() const { return PlayerUid != 0; }

protected:
    virtual void BeginPlay() override;

private:
    void UpdatePlayerUidFromPlayerState() const;

    mutable int64 PlayerUid = 0;
    mutable bool bPlayerUidCached = false;
};
