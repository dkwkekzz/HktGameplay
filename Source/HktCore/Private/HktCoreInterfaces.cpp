// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreInterfaces.h"
#include "VM/HktMasterStash.h"
#include "VM/HktVisibleStash.h"
#include "VM/HktVMProcessor.h"

TUniquePtr<IHktVMProcessorInterface> CreateVMProcessor(IHktStashInterface* InStash)
{
    TUniquePtr<FHktVMProcessor> VMProcessor = MakeUnique<FHktVMProcessor>();
    VMProcessor->Initialize(InStash);
    return VMProcessor;
}

TUniquePtr<IHktMasterStashInterface> CreateMasterStash()
{
    return MakeUnique<FHktMasterStash>();
}

TUniquePtr<IHktVisibleStashInterface> CreateVisibleStash()
{
    return MakeUnique<FHktVisibleStash>();
}
