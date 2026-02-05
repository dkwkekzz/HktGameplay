// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPhysics.h"

TUniquePtr<FHktPhysicsWorld> CreatePhysicsWorld(IHktStashInterface* InStash)
{
    TUniquePtr<FHktPhysicsWorld> World = MakeUnique<FHktPhysicsWorld>();
    World->Initialize(InStash);
    return World;
}
