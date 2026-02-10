// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"

struct HKTCORE_API FHktEvent
{
    FGameplayTag EventTag;
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    FVector Location = FVector::ZeroVector;
    int32 Param0 = 0;
    int32 Param1 = 0;
};

struct HKTCORE_API FHktPhysicsEvent
{
    FHktEntityId EntityA;
    FHktEntityId EntityB;
    FVector ContactPoint;
};

struct HKTCORE_API FHktSimulationEvent
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    float DeltaSeconds = 0.0f;
    TArray<int64> RemovedOwnerIds;
    TArray<FHktEvent> Events;
};

struct HKTCORE_API FHktEntityState
{
    int32 EntityId = InvalidEntityId;
    TArray<int8> Properties;
    FGameplayTagContainer Tags;
};

struct HKTCORE_API FHktSimulationState
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    TArray<FHktEntityState> Entities;
};

struct HKTCORE_API FHktOwnerState
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    TArray<FHktEvent> ProcessingEvents;
    TArray<FHktEntityState> Entities;
};
