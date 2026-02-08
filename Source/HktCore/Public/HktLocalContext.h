// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HktEntityTypes.h"
#include "HktWorldState.h"

namespace Hkt
{
    // ==================================================================================
    // Local Context - 트랜잭션 로컬 캐시
    // ==================================================================================

    class HKTCORE_API FHktLocalContext
    {
    public:
        FHktLocalContext(FHktWorldState* InWorldState);

        const FComponentData& Read(FEntityID EntityID);
        void Write(FEntityID EntityID, const FComponentData& InData);
        void CommitChanges();

    private:
        FHktWorldState* WorldState;
        TMap<FEntityID, FComponentData> LocalCache;
        TArray<FEntityID> DirtyEntities;
    };
}
