// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVMProgram.h"

// ============================================================================
// FHktVMProgramRegistry
// ============================================================================

FHktVMProgramRegistry& FHktVMProgramRegistry::Get()
{
    static FHktVMProgramRegistry Instance;
    return Instance;
}

const FHktVMProgram* FHktVMProgramRegistry::FindProgram(const FGameplayTag& Tag) const
{
    FRWScopeLock ReadLock(Lock, SLT_ReadOnly);
    if (const TSharedRef<FHktVMProgram>* Found = Programs.Find(Tag))
    {
        return &Found->Get();
    }
    return nullptr;
}

void FHktVMProgramRegistry::RegisterProgram(TSharedRef<FHktVMProgram> Program)
{
    FRWScopeLock WriteLock(Lock, SLT_Write);
    FGameplayTag Tag = Program->Tag;
    Programs.Add(Tag, Program);
}

void FHktVMProgramRegistry::Clear()
{
    FRWScopeLock WriteLock(Lock, SLT_Write);
    Programs.Empty();
}

