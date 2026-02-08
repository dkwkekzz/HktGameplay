// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "HktEntityTypes.h"

namespace Hkt
{
    // ==================================================================================
    // State Management - 데이터 저장소
    // ==================================================================================

    class HKTCORE_API FHktWorldState
    {
    public:
        const FComponentData* GetComponent(FEntityID EntityID) const;
        FComponentData* GetComponentMutable(FEntityID EntityID);
        void CommitComponent(FEntityID EntityID, const FComponentData& NewData);
        const TMap<FEntityID, FComponentData>& GetAllEntities() const;

    private:
        TMap<FEntityID, FComponentData> EntityComponents;
    };
}
