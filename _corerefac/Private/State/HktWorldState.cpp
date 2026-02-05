// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "State/HktWorldState.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

const FGameplayTagContainer FHktWorldState::EmptyTagContainer;

FHktWorldState::FHktWorldState()
{
    Properties.SetNum(MaxProperties);
    for (int32 i = 0; i < MaxProperties; ++i)
    {
        Properties[i].SetNumZeroed(MaxEntities);
    }

    EntityTags.SetNum(MaxEntities);
    ValidEntities.Init(false, MaxEntities);
    EntityCreationFrame.SetNumZeroed(MaxEntities);
}

// ========== Entity Management ==========

FHktEntityId FHktWorldState::AllocateEntity()
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
        UE_LOG(LogTemp, Error, TEXT("[WorldState] Entity limit reached!"));
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

    UE_LOG(LogTemp, Verbose, TEXT("[WorldState] Entity %d allocated"), Id.RawValue);
    return Id;
}

void FHktWorldState::FreeEntity(FHktEntityId Entity)
{
    if (Entity.RawValue >= 0 && Entity.RawValue < MaxEntities && ValidEntities[Entity.RawValue])
    {
        ValidEntities[Entity.RawValue] = false;
        EntityTags[Entity.RawValue].Reset();
        FreeList.Add(Entity);

        UE_LOG(LogTemp, Verbose, TEXT("[WorldState] Entity %d freed"), Entity.RawValue);
    }
}

bool FHktWorldState::IsValidEntity(FHktEntityId Entity) const
{
    return Entity.RawValue >= 0 && Entity.RawValue < MaxEntities && ValidEntities[Entity.RawValue];
}

int32 FHktWorldState::GetEntityCount() const
{
    int32 Count = 0;
    for (int32 i = 0; i < MaxEntities; ++i)
    {
        if (ValidEntities[i])
            Count++;
    }
    return Count;
}

// ========== Property API ==========

int32 FHktWorldState::GetProperty(FHktEntityId Entity, uint16 PropertyId) const
{
    if (!IsValidEntity(Entity) || PropertyId >= MaxProperties)
        return 0;
    return Properties[PropertyId][Entity.RawValue];
}

void FHktWorldState::SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value)
{
    if (Entity.RawValue < 0 || Entity.RawValue >= MaxEntities || PropertyId >= MaxProperties)
        return;

    if (!ValidEntities[Entity.RawValue])
        return;

    Properties[PropertyId][Entity.RawValue] = Value;
}

// ========== Batch Write ==========

void FHktWorldState::ApplyWrites(const TArray<FPendingWrite>& Writes)
{
    for (const auto& W : Writes)
    {
        SetProperty(W.Entity, W.PropertyId, W.Value);
    }
}

// ========== Tag API ==========

const FGameplayTagContainer& FHktWorldState::GetTags(FHktEntityId Entity) const
{
    if (!IsValidEntity(Entity))
        return EmptyTagContainer;
    return EntityTags[Entity.RawValue];
}

void FHktWorldState::SetTags(FHktEntityId Entity, const FGameplayTagContainer& Tags)
{
    if (Entity.RawValue < 0 || Entity.RawValue >= MaxEntities)
        return;

    if (!ValidEntities[Entity.RawValue])
        return;

    EntityTags[Entity.RawValue] = Tags;
}

void FHktWorldState::AddTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity) || !Tag.IsValid())
        return;

    if (!EntityTags[Entity.RawValue].HasTagExact(Tag))
    {
        EntityTags[Entity.RawValue].AddTag(Tag);
    }
}

void FHktWorldState::RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity) || !Tag.IsValid())
        return;

    if (EntityTags[Entity.RawValue].HasTagExact(Tag))
    {
        EntityTags[Entity.RawValue].RemoveTag(Tag);
    }
}

bool FHktWorldState::HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityTags[Entity.RawValue].HasTag(Tag);
}

bool FHktWorldState::HasTagExact(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityTags[Entity.RawValue].HasTagExact(Tag);
}

bool FHktWorldState::HasAnyTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityTags[Entity.RawValue].HasAny(Tags);
}

bool FHktWorldState::HasAllTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityTags[Entity.RawValue].HasAll(Tags);
}

FGameplayTag FHktWorldState::GetFirstTagWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const
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

FGameplayTagContainer FHktWorldState::GetTagsWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const
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

// ========== Frame Management ==========

void FHktWorldState::MarkFrameCompleted(int32 FrameNumber)
{
    CompletedFrameNumber = FrameNumber;
}

// ========== Iteration ==========

void FHktWorldState::ForEachEntity(TFunctionRef<void(FHktEntityId)> Callback) const
{
    for (int32 E = 0; E < MaxEntities; ++E)
    {
        if (ValidEntities[E])
        {
            Callback(FHktEntityId(E));
        }
    }
}

// ========== Checksum ==========

uint32 FHktWorldState::CalculateChecksum() const
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

uint32 FHktWorldState::CalculatePartialChecksum(const TArray<FHktEntityId>& Entities) const
{
    uint32 Checksum = 0;

    for (FHktEntityId E : Entities)
    {
        if (!IsValidEntity(E))
            continue;

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
    }

    return Checksum;
}

// ========== Snapshot & Serialization ==========

FHktEntitySnapshot FHktWorldState::CreateEntitySnapshot(FHktEntityId Entity) const
{
    FHktEntitySnapshot Snapshot;

    if (!IsValidEntity(Entity))
        return Snapshot;

    Snapshot.EntityId = Entity;
    Snapshot.Properties.SetNum(MaxProperties);

    for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
    {
        Snapshot.Properties[PropId] = Properties[PropId][Entity.RawValue];
    }

    Snapshot.Tags = EntityTags[Entity.RawValue];

    return Snapshot;
}

TArray<FHktEntitySnapshot> FHktWorldState::CreateSnapshots(const TArray<FHktEntityId>& Entities) const
{
    TArray<FHktEntitySnapshot> Snapshots;
    Snapshots.Reserve(Entities.Num());

    for (FHktEntityId E : Entities)
    {
        FHktEntitySnapshot Snap = CreateEntitySnapshot(E);
        if (Snap.IsValid())
        {
            Snapshots.Add(MoveTemp(Snap));
        }
    }

    return Snapshots;
}

TArray<uint8> FHktWorldState::SerializeFullState() const
{
    TArray<uint8> Data;
    FMemoryWriter Writer(Data);

    int32 Frame = CompletedFrameNumber;
    int32 NextId = NextEntityId;
    Writer << Frame;
    Writer << NextId;

    int32 NumValid = GetEntityCount();
    Writer << NumValid;

    for (int32 E = 0; E < MaxEntities; ++E)
    {
        if (ValidEntities[E])
        {
            int32 EntityInt = E;
            Writer << EntityInt;

            for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
            {
                int32 PropValue = Properties[PropId][E];
                Writer << PropValue;
            }

            FGameplayTagContainer Tags = EntityTags[E];
            bool bSerializeSuccess = false;
            Tags.NetSerialize(Writer, nullptr, bSerializeSuccess);
        }
    }

    return Data;
}

void FHktWorldState::DeserializeFullState(const TArray<uint8>& Data)
{
    if (Data.Num() == 0)
        return;

    FMemoryReader Reader(Data);

    int32 Frame, NextId;
    Reader << Frame;
    Reader << NextId;

    CompletedFrameNumber = Frame;
    NextEntityId = NextId;

    ValidEntities.Init(false, MaxEntities);
    FreeList.Reset();

    int32 NumValid = 0;
    Reader << NumValid;

    for (int32 i = 0; i < NumValid; ++i)
    {
        int32 EntityInt;
        Reader << EntityInt;

        FHktEntityId E(EntityInt);
        ValidEntities[E.RawValue] = true;

        for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
        {
            int32 PropValue;
            Reader << PropValue;
            Properties[PropId][E.RawValue] = PropValue;
        }

        bool bSerializeSuccess = false;
        EntityTags[E.RawValue].NetSerialize(Reader, nullptr, bSerializeSuccess);
    }

    UE_LOG(LogTemp, Log, TEXT("[WorldState] Deserialized: Frame=%d, Entities=%d"),
        CompletedFrameNumber, NumValid);
}

// ========== Position Access ==========

bool FHktWorldState::TryGetPosition(FHktEntityId Entity, FVector& OutPosition) const
{
    if (!IsValidEntity(Entity))
    {
        return false;
    }

    OutPosition.X = static_cast<float>(GetProperty(Entity, PropertyId::PosX));
    OutPosition.Y = static_cast<float>(GetProperty(Entity, PropertyId::PosY));
    OutPosition.Z = static_cast<float>(GetProperty(Entity, PropertyId::PosZ));

    return true;
}

void FHktWorldState::SetPosition(FHktEntityId Entity, const FVector& Position)
{
    if (!IsValidEntity(Entity))
    {
        return;
    }

    SetProperty(Entity, PropertyId::PosX, FMath::RoundToInt(Position.X));
    SetProperty(Entity, PropertyId::PosY, FMath::RoundToInt(Position.Y));
    SetProperty(Entity, PropertyId::PosZ, FMath::RoundToInt(Position.Z));
}

// ========== Radius Query ==========

void FHktWorldState::ForEachEntityInRadius(FHktEntityId Center, int32 RadiusCm, TFunctionRef<void(FHktEntityId)> Callback) const
{
    if (!IsValidEntity(Center))
        return;

    int32 CX = GetProperty(Center, PropertyId::PosX);
    int32 CY = GetProperty(Center, PropertyId::PosY);
    int32 CZ = GetProperty(Center, PropertyId::PosZ);
    int64 RadiusSq = static_cast<int64>(RadiusCm) * RadiusCm;

    ForEachEntity([&](FHktEntityId E)
    {
        if (E.RawValue == Center.RawValue) return;

        int32 EX = GetProperty(E, PropertyId::PosX);
        int32 EY = GetProperty(E, PropertyId::PosY);
        int32 EZ = GetProperty(E, PropertyId::PosZ);

        int64 DX = EX - CX;
        int64 DY = EY - CY;
        int64 DZ = EZ - CZ;

        if (DX*DX + DY*DY + DZ*DZ <= RadiusSq)
        {
            Callback(E);
        }
    });
}

// ========== Frame Validation ==========

bool FHktWorldState::ValidateEntityFrame(FHktEntityId Entity, int32 FrameNumber) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityCreationFrame[Entity] <= FrameNumber;
}
