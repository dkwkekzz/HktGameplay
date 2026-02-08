// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStash.h"

const FGameplayTagContainer FHktStashBase::EmptyTagContainer;

FHktStashBase::FHktStashBase()
{
    Properties.SetNum(MaxProperties);
    for (int32 i = 0; i < MaxProperties; ++i)
    {
        Properties[i].SetNumZeroed(MaxEntities);
    }
    
    EntityTags.SetNum(MaxEntities);
    ValidEntities.Init(false, MaxEntities);
}

FHktEntityId FHktStashBase::AllocateEntity()
{
    FHktEntityId Id;
    
    if (FreeList.Num() > 0)
    {
        Id = FreeList.Pop();
    }
    else if (NextEntityId < MaxEntities)
    {
        Id = NextEntityId++;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[Stash] Entity limit reached!"));
        return InvalidEntityId;
    }
    
    ValidEntities[Id] = true;
    
    // 속성 초기화
    for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
    {
        Properties[PropId][Id] = 0;
    }
    
    // 태그 초기화
    EntityTags[Id].Reset();
    
    OnEntityDirty(Id);
    
    UE_LOG(LogTemp, Verbose, TEXT("[Stash] Entity %d allocated"), Id.RawValue);
    return Id;
}

void FHktStashBase::FreeEntity(FHktEntityId Entity)
{
    if (Entity.RawValue >= 0 && Entity.RawValue < MaxEntities && ValidEntities[Entity.RawValue])
    {
        ValidEntities[Entity.RawValue] = false;
        EntityTags[Entity.RawValue].Reset();
        FreeList.Add(Entity);
        OnEntityDirty(Entity);
        
        UE_LOG(LogTemp, Verbose, TEXT("[Stash] Entity %d freed"), Entity.RawValue);
    }
}

bool FHktStashBase::IsValidEntity(FHktEntityId Entity) const
{
    return Entity.RawValue >= 0 && Entity.RawValue < MaxEntities && ValidEntities[Entity.RawValue];
}

int32 FHktStashBase::GetProperty(FHktEntityId Entity, uint16 PropertyId) const
{
    if (!IsValidEntity(Entity) || PropertyId >= MaxProperties)
        return 0;
    return Properties[PropertyId][Entity.RawValue];
}

void FHktStashBase::SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value)
{
    if (Entity.RawValue < 0 || Entity.RawValue >= MaxEntities || PropertyId >= MaxProperties)
        return;
    
    // 자동 생성 모드 (VisibleStash용)
    if (bAutoCreateOnSet && !ValidEntities[Entity.RawValue])
    {
        ValidEntities[Entity.RawValue] = true;
        if (Entity.RawValue >= NextEntityId)
            NextEntityId = Entity.RawValue + 1;
        
        for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
        {
            Properties[PropId][Entity.RawValue] = 0;
        }
        EntityTags[Entity.RawValue].Reset();
    }
    
    if (!ValidEntities[Entity.RawValue])
        return;
    
    if (Properties[PropertyId][Entity.RawValue] != Value)
    {
        Properties[PropertyId][Entity.RawValue] = Value;
        OnEntityDirty(Entity);
    }
}

// ========== Tag API ==========

const FGameplayTagContainer& FHktStashBase::GetTags(FHktEntityId Entity) const
{
    if (!IsValidEntity(Entity))
        return EmptyTagContainer;
    return EntityTags[Entity.RawValue];
}

void FHktStashBase::SetTags(FHktEntityId Entity, const FGameplayTagContainer& Tags)
{
    if (Entity.RawValue < 0 || Entity.RawValue >= MaxEntities)
        return;
    
    if (bAutoCreateOnSet && !ValidEntities[Entity.RawValue])
    {
        ValidEntities[Entity.RawValue] = true;
        if (Entity.RawValue >= NextEntityId)
            NextEntityId = Entity.RawValue + 1;
        
        for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
        {
            Properties[PropId][Entity.RawValue] = 0;
        }
    }
    
    if (!ValidEntities[Entity.RawValue])
        return;
    
    EntityTags[Entity.RawValue] = Tags;
    OnEntityDirty(Entity);
}

void FHktStashBase::AddTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity) || !Tag.IsValid())
        return;
    
    if (!EntityTags[Entity.RawValue].HasTagExact(Tag))
    {
        EntityTags[Entity.RawValue].AddTag(Tag);
        OnEntityDirty(Entity);
    }
}

void FHktStashBase::RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity) || !Tag.IsValid())
        return;
    
    if (EntityTags[Entity.RawValue].HasTagExact(Tag))
    {
        EntityTags[Entity.RawValue].RemoveTag(Tag);
        OnEntityDirty(Entity);
    }
}

bool FHktStashBase::HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityTags[Entity.RawValue].HasTag(Tag);
}

bool FHktStashBase::HasTagExact(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityTags[Entity.RawValue].HasTagExact(Tag);
}

bool FHktStashBase::HasAnyTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityTags[Entity.RawValue].HasAny(Tags);
}

bool FHktStashBase::HasAllTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityTags[Entity.RawValue].HasAll(Tags);
}

FGameplayTag FHktStashBase::GetFirstTagWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const
{
    if (!IsValidEntity(Entity) || !ParentTag.IsValid())
        return FGameplayTag();
    
    for (const FGameplayTag& Tag : EntityTags[Entity.RawValue])
    {
        if (Tag.MatchesTag(ParentTag))
        {
            return Tag;
        }
    }
    return FGameplayTag();
}

FGameplayTagContainer FHktStashBase::GetTagsWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const
{
    FGameplayTagContainer Result;
    if (!IsValidEntity(Entity) || !ParentTag.IsValid())
        return Result;
    
    for (const FGameplayTag& Tag : EntityTags[Entity.RawValue])
    {
        if (Tag.MatchesTag(ParentTag))
        {
            Result.AddTag(Tag);
        }
    }
    return Result;
}

int32 FHktStashBase::GetEntityCount() const
{
    int32 Count = 0;
    for (int32 i = 0; i < MaxEntities; ++i)
    {
        if (ValidEntities[i])
            Count++;
    }
    return Count;
}

void FHktStashBase::MarkFrameCompleted(int32 FrameNumber)
{
    CompletedFrameNumber = FrameNumber;
}

void FHktStashBase::ForEachEntity(TFunctionRef<void(FHktEntityId)> Callback) const
{
    for (int32 E = 0; E < MaxEntities; ++E)
    {
        if (ValidEntities[E])
        {
            Callback(FHktEntityId(E));
        }
    }
}

uint32 FHktStashBase::CalculateChecksum() const
{
    uint32 Checksum = 0;
    
    ForEachEntity([&](FHktEntityId E)
    {
        for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
        {
            Checksum ^= Properties[PropId][E.RawValue];
            Checksum = (Checksum << 1) | (Checksum >> 31);
        }
        
        for (const FGameplayTag& Tag : EntityTags[E.RawValue])
        {
            Checksum ^= GetTypeHash(Tag);
            Checksum = (Checksum << 1) | (Checksum >> 31);
        }
        
        Checksum ^= E.RawValue;
    });
    
    Checksum ^= CompletedFrameNumber;
    return Checksum;
}
