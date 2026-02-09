// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMasterStashComponent.h"
#include "HktCoreInterfaces.h"

UHktMasterStashComponent::UHktMasterStashComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHktMasterStashComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // HktCore에서 MasterStash 인스턴스 생성
    Stash = CreateMasterStash();
    
    if (Stash)
    {
        UE_LOG(LogTemp, Log, TEXT("[MasterStashComponent] Initialized"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[MasterStashComponent] Failed to create MasterStash"));
    }
}

void UHktMasterStashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Stash.Reset();
    Super::EndPlay(EndPlayReason);
}
