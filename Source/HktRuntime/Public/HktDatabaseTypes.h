// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktRuntimeTypes.h"
#include "HktDatabaseTypes.generated.h"

/**
 * FHktPlayerRecord - 플레이어의 영구 저장 데이터
 */
USTRUCT(BlueprintType)
struct HKTRUNTIME_API FHktPlayerRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int64 PlayerUid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    FDateTime LastLoginTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    FDateTime CreatedTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    FVector LastPosition;

    TArray<FHktEvent> ActiveEvents;
    TArray<FHktEntityState> EntityStates;

    FHktPlayerRecord()
    {
        CreatedTime = FDateTime::UtcNow();
        LastLoginTime = CreatedTime;
    }

    bool IsValid() const { return PlayerUid != 0; }
    bool HasEntities() const { return EntityStates.Num() > 0; }
};
