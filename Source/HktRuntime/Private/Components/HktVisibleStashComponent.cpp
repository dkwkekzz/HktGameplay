// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVisibleStashComponent.h"
#include "HktCoreInterfaces.h"

UHktVisibleStashComponent::UHktVisibleStashComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHktVisibleStashComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // HktCore에서 VisibleStash 인스턴스 생성
    Stash = CreateVisibleStash();
    
    if (Stash)
    {
        UE_LOG(LogTemp, Log, TEXT("[VisibleStashComponent] Initialized"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[VisibleStashComponent] Failed to create VisibleStash"));
    }
}

void UHktVisibleStashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Stash.Reset();
    Super::EndPlay(EndPlayReason);
}
