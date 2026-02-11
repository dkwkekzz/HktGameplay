// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldPlayerComponent.h"
// TODO: 이벤트로 연결하여 Actor를 직접 참조하지 않는다.
#include "Actors/HktInGamePlayerController.h"

UHktWorldPlayerComponent::UHktWorldPlayerComponent()
{ 
    PrimaryComponentTick.bCanEverTick = false; 
}

void UHktWorldPlayerComponent::SendFrameBatch(const FHktRuntimeBatch& Batch)
{
    if (AHktInGamePlayerController* PC = GetOwnerPC()) 
    { 
        PC->Client_ReceiveFrameBatch(Batch); 
    }
}

void UHktWorldPlayerComponent::SendInitialSimulationState(const FHktRuntimeSimulationState& InitialState)
{
    if (AHktInGamePlayerController* PC = GetOwnerPC()) 
    { 
        PC->Client_ReceiveInitialState(InitialState); 
    }
}

AHktInGamePlayerController* UHktWorldPlayerComponent::GetOwnerPC() const
{
    return Cast<AHktInGamePlayerController>(GetOwner());
}
