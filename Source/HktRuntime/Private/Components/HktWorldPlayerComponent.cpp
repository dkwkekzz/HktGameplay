// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldPlayerComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

UHktWorldPlayerComponent::UHktWorldPlayerComponent()
{ 
    PrimaryComponentTick.bCanEverTick = false; 
}

void UHktWorldPlayerComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // PlayerState가 이미 존재할 수 있으므로 초기 UID 계산 시도
    UpdatePlayerUidFromPlayerState();
}

int64 UHktWorldPlayerComponent::GetPlayerUid() const
{
    if (!bPlayerUidCached)
    {
        UpdatePlayerUidFromPlayerState();
    }
    return PlayerUid;
}

void UHktWorldPlayerComponent::UpdatePlayerUidFromPlayerState() const
{
    if (bPlayerUidCached)
    {
        return;
    }

    PlayerUid = 0;
    
    if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
    {
        if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
        {
            FUniqueNetIdRepl UniqueId = PS->GetUniqueId();
            if (UniqueId.IsValid())
            {
                PlayerUid = GetTypeHash(UniqueId->ToString());
            }
        }
    }
    
    bPlayerUidCached = true;
}

bool UHktWorldPlayerComponent::IsInitialized() const
{
    return PlayerUid != 0;
}

void UHktWorldPlayerComponent::InvalidatePlayerUidCache()
{
    bPlayerUidCached = false;
    PlayerUid = 0;
}
