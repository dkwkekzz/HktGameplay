// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktLocalContext.h"

namespace Hkt
{
    FHktLocalContext::FHktLocalContext(FHktWorldState* InWorldState)
        : WorldState(InWorldState)
    {
    }

    const FComponentData& FHktLocalContext::Read(FEntityID EntityID)
    {
        if (FComponentData* Cached = LocalCache.Find(EntityID)) return *Cached;
        if (const FComponentData* Original = WorldState->GetComponent(EntityID)) return LocalCache.Add(EntityID, *Original);
        static FComponentData DefaultData;
        return DefaultData;
    }

    void FHktLocalContext::Write(FEntityID EntityID, const FComponentData& InData)
    {
        LocalCache.Add(EntityID, InData);
        DirtyEntities.AddUnique(EntityID);
    }

    void FHktLocalContext::CommitChanges()
    {
        for (FEntityID ID : DirtyEntities)
        {
            WorldState->CommitComponent(ID, LocalCache[ID]);
        }
        LocalCache.Empty();
        DirtyEntities.Empty();
    }
}
