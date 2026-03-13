// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldState.h"
#include "HktCoreProperties.h"
#include "HktSimulationLimits.h"

// ============================================================================
// Slot Management (private)
// ============================================================================

int32 FHktWorldState::AllocateSlot(FHktEntityId EntityId)
{
    int32 Slot;
    if (FreeSlots.Num() > 0)
    {
        Slot = FreeSlots.Pop();
        SlotToEntity[Slot] = EntityId;
        TagContainers[Slot].Reset();
        OwnerUids[Slot] = 0;
    }
    else
    {
        Slot = SlotToEntity.Num();
        SlotToEntity.Add(EntityId);
        Data.AddZeroed(Stride);
        TagContainers.Add({});
        OwnerUids.Add(0);
    }
    FMemory::Memzero(Data.GetData() + Slot * Stride, Stride * sizeof(int32));
    ActiveCount++;
    return Slot;
}

void FHktWorldState::FreeSlot(int32 Slot)
{
    SlotToEntity[Slot] = InvalidEntityId;
    TagContainers[Slot].Reset();
    OwnerUids[Slot] = 0;
    FreeSlots.Add(Slot);
    ActiveCount--;
}

// ============================================================================
// Lifecycle
// ============================================================================

void FHktWorldState::Initialize()
{
    EntitySlots.Reserve(HktLimits::MaxEntities);
    ActiveEvents.Reserve(HktLimits::MaxActiveEvents);

    constexpr int32 ReserveCount = 2176;  // 512 + 1024 + 512 + 128
    Data.Reserve(ReserveCount * Stride);
    SlotToEntity.Reserve(ReserveCount);
    TagContainers.Reserve(ReserveCount);
    OwnerUids.Reserve(ReserveCount);
}

FHktEntityId FHktWorldState::AllocateEntity(FHktTypeId TypeId)
{
    check(TypeId > HktType::None && TypeId < HktType::MaxTypes);
    FHktEntityId NewId = NextEntityId++;
    if (NewId >= EntitySlots.Num())
    {
        int32 OldNum = EntitySlots.Num();
        EntitySlots.SetNum(NewId + 1);
        for (int32 i = OldNum; i < EntitySlots.Num(); ++i)
            EntitySlots[i] = -1;
    }
    int32 Slot = AllocateSlot(NewId);
    EntitySlots[NewId] = Slot;
    Set(Slot, PropertyId::EntityType, TypeId);
    return NewId;
}

void FHktWorldState::RemoveEntity(FHktEntityId Id)
{
    if (!IsValidEntity(Id)) return;
    FreeSlot(EntitySlots[Id]);
    EntitySlots[Id] = -1;
}

int32 FHktWorldState::GetEntityCount() const
{
    return ActiveCount;
}

// ============================================================================
// DTO
// ============================================================================

FHktEntityState FHktWorldState::ExtractEntityState(FHktEntityId Id) const
{
    FHktEntityState S;
    S.EntityId = Id;
    if (!IsValidEntity(Id)) return S;
    int32 Slot = EntitySlots[Id];
    S.TypeId = static_cast<FHktTypeId>(Get(Slot, PropertyId::EntityType));
    S.Data.SetNumUninitialized(Stride);
    FMemory::Memcpy(S.Data.GetData(), EntityData(Slot), Stride * sizeof(int32));
    S.Tags = TagContainers[Slot];
    S.OwnerUid = OwnerUids[Slot];
    return S;
}

FHktEntityId FHktWorldState::ImportEntityState(const FHktEntityState& InState)
{
    FHktEntityId Id = AllocateEntity(InState.TypeId);
    int32 Slot = EntitySlots[Id];
    int32 N = FMath::Min(Stride, InState.Data.Num());
    FMemory::Memcpy(EntityData(Slot), InState.Data.GetData(), N * sizeof(int32));
    TagContainers[Slot] = InState.Tags;
    OwnerUids[Slot] = InState.OwnerUid;
    return Id;
}

void FHktWorldState::ImportEntityStateWithId(const FHktEntityState& InState)
{
    FHktEntityId Id = InState.EntityId;
    if (Id >= EntitySlots.Num())
    {
        int32 OldNum = EntitySlots.Num();
        EntitySlots.SetNum(Id + 1);
        for (int32 i = OldNum; i < EntitySlots.Num(); ++i)
            EntitySlots[i] = -1;
    }
    int32 Slot = AllocateSlot(Id);
    EntitySlots[Id] = Slot;
    int32 N = FMath::Min(Stride, InState.Data.Num());
    FMemory::Memcpy(EntityData(Slot), InState.Data.GetData(), N * sizeof(int32));
    TagContainers[Slot] = InState.Tags;
    OwnerUids[Slot] = InState.OwnerUid;
}

// ============================================================================
// UndoDiff
// ============================================================================

void FHktWorldState::UndoDiff(const FHktSimulationDiff& Diff)
{
    for (const FHktEntityState& S : Diff.SpawnedEntities)
        RemoveEntity(S.EntityId);

    if (Diff.PrevNextEntityId != InvalidEntityId)
        NextEntityId = Diff.PrevNextEntityId;

    for (const FHktEntityState& S : Diff.RemovedEntityStates)
        ImportEntityStateWithId(S);

    for (const FHktPropertyDelta& D : Diff.PropertyDeltas)
        SetProperty(D.EntityId, D.PropertyId, D.OldValue);

    for (const FHktOwnerDelta& D : Diff.OwnerDeltas)
        SetOwnerUid(D.EntityId, D.OldOwnerUid);

    for (const FHktTagDelta& D : Diff.TagDeltas)
    {
        if (!IsValidEntity(D.EntityId)) continue;
        TagContainers[EntitySlots[D.EntityId]] = D.OldTags;
    }

    FrameNumber = Diff.FrameNumber - 1;
}

// ============================================================================
// CopyFrom
// ============================================================================

void FHktWorldState::CopyFrom(const FHktWorldState& Other)
{
    FrameNumber = Other.FrameNumber;
    RandomSeed = Other.RandomSeed;
    NextEntityId = Other.NextEntityId;
    EntitySlots = Other.EntitySlots;
    ActiveEvents = Other.ActiveEvents;

    Data = Other.Data;
    SlotToEntity = Other.SlotToEntity;
    FreeSlots = Other.FreeSlots;
    ActiveCount = Other.ActiveCount;
    TagContainers = Other.TagContainers;
    OwnerUids = Other.OwnerUids;
}

// ============================================================================
// Serialization
// ============================================================================

bool FHktWorldState::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
    Ar << FrameNumber << RandomSeed << NextEntityId;

    if (Ar.IsSaving())
    {
        int32 TotalEntities = ActiveCount;
        Ar << TotalEntities;

        ForEachEntity([&](FHktEntityId Id, int32 Slot)
        {
            Ar << Id;
            uint8 TypeByte = static_cast<uint8>(Get(Slot, PropertyId::EntityType));
            Ar << TypeByte;

            for (int32 P = 0; P < Stride; ++P)
            {
                int32 Val = EntityData(Slot)[P];
                Ar << Val;
            }

            TagContainers[Slot].NetSerialize(Ar, Map, bOutSuccess);
            Ar << OwnerUids[Slot];
        });
    }
    else // IsLoading
    {
        EntitySlots.Reset();
        Data.Reset();
        SlotToEntity.Reset();
        FreeSlots.Reset();
        TagContainers.Reset();
        OwnerUids.Reset();
        ActiveCount = 0;

        int32 TotalEntities; Ar << TotalEntities;
        for (int32 i = 0; i < TotalEntities; ++i)
        {
            FHktEntityId Id; Ar << Id;
            uint8 TypeByte;  Ar << TypeByte;

            if (Id >= EntitySlots.Num())
            {
                int32 OldNum = EntitySlots.Num();
                EntitySlots.SetNum(Id + 1);
                for (int32 j = OldNum; j < EntitySlots.Num(); ++j)
                    EntitySlots[j] = -1;
            }

            int32 Slot = AllocateSlot(Id);
            EntitySlots[Id] = Slot;

            for (int32 P = 0; P < Stride; ++P)
                Ar << EntityData(Slot)[P];

            TagContainers[Slot].NetSerialize(Ar, Map, bOutSuccess);
            Ar << OwnerUids[Slot];
        }
    }

    Ar << ActiveEvents;
    return true;
}

// ============================================================================
// Tag Access (EntityId 기반)
// ============================================================================

const FGameplayTagContainer& FHktWorldState::GetTags(FHktEntityId Entity) const
{
    static FGameplayTagContainer Empty;
    if (!IsValidEntity(Entity)) return Empty;
    return TagContainers[EntitySlots[Entity]];
}

void FHktWorldState::AddTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity)) return;
    TagContainers[EntitySlots[Entity]].AddTag(Tag);
}

void FHktWorldState::RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity)) return;
    TagContainers[EntitySlots[Entity]].RemoveTag(Tag);
}

bool FHktWorldState::HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    if (!IsValidEntity(Entity)) return false;
    return TagContainers[EntitySlots[Entity]].HasTag(Tag);
}
