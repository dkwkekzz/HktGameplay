// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldState.h"

namespace Hkt
{
    const FComponentData* FHktWorldState::GetComponent(FEntityID EntityID) const
    {
        return EntityComponents.Find(EntityID);
    }

    FComponentData* FHktWorldState::GetComponentMutable(FEntityID EntityID)
    {
        return EntityComponents.Find(EntityID);
    }

    void FHktWorldState::CommitComponent(FEntityID EntityID, const FComponentData& NewData)
    {
        EntityComponents.Add(EntityID, NewData);
    }

    const TMap<FEntityID, FComponentData>& FHktWorldState::GetAllEntities() const
    {
        return EntityComponents;
    }
}
