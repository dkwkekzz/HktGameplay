// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldState.h"
#include "HktCoreProperties.h"
#include "HktSimulationLimits.h"

// ============================================================================
// FHktSchemaRegistry
// ============================================================================

void FHktSchemaRegistry::Initialize()
{
    {
        auto& S = Schemas[HktType::Unit];
        S.TypeId = HktType::Unit;
        for (uint16 P : {
            PropertyId::PosX, PropertyId::PosY, PropertyId::PosZ, PropertyId::RotYaw,
            PropertyId::TargetPosX, PropertyId::TargetPosY, PropertyId::TargetPosZ, PropertyId::Param0, PropertyId::Param1,
            PropertyId::MoveTargetX, PropertyId::MoveTargetY, PropertyId::MoveTargetZ,
            PropertyId::MoveForce, PropertyId::IsMoving, PropertyId::MaxSpeed,
            PropertyId::VelX, PropertyId::VelY, PropertyId::VelZ,
            PropertyId::Mass, PropertyId::CollisionRadius,
            PropertyId::Health, PropertyId::MaxHealth,
            PropertyId::AttackPower, PropertyId::Defense,
            PropertyId::Team, PropertyId::Mana, PropertyId::MaxMana,
            PropertyId::OwnerEntity, PropertyId::EntityType, PropertyId::EntitySpawnTag,
            PropertyId::AnimState, PropertyId::VisualState,
            PropertyId::IsNPC, PropertyId::SpawnFlowTag })
            S.MarkValid(P);
    }
    {
        auto& S = Schemas[HktType::Projectile];
        S.TypeId = HktType::Projectile;
        for (uint16 P : {
            PropertyId::PosX, PropertyId::PosY, PropertyId::PosZ,
            PropertyId::TargetPosX, PropertyId::TargetPosY, PropertyId::TargetPosZ, PropertyId::Param0, PropertyId::Param1,
            PropertyId::MoveTargetX, PropertyId::MoveTargetY, PropertyId::MoveTargetZ,
            PropertyId::MoveForce, PropertyId::IsMoving, PropertyId::MaxSpeed,
            PropertyId::VelX, PropertyId::VelY, PropertyId::VelZ,
            PropertyId::Mass, PropertyId::CollisionRadius,
            PropertyId::OwnerEntity, PropertyId::EntityType, PropertyId::EntitySpawnTag,
            PropertyId::Team })
            S.MarkValid(P);
    }
    {
        auto& S = Schemas[HktType::Equipment];
        S.TypeId = HktType::Equipment;
        for (uint16 P : {
            PropertyId::PosX, PropertyId::PosY, PropertyId::PosZ,
            PropertyId::OwnerEntity, PropertyId::AttackPower,
            PropertyId::TargetPosX, PropertyId::TargetPosY, PropertyId::TargetPosZ, PropertyId::Param0, PropertyId::Param1,
            PropertyId::MoveTargetX, PropertyId::MoveTargetY, PropertyId::MoveTargetZ,
            PropertyId::MoveForce, PropertyId::IsMoving, PropertyId::MaxSpeed,
            PropertyId::VelX, PropertyId::VelY, PropertyId::VelZ,
            PropertyId::Mass, PropertyId::CollisionRadius,
            PropertyId::EntityType, PropertyId::EntitySpawnTag,
            PropertyId::Defense,
            PropertyId::ItemState, PropertyId::ItemId, PropertyId::BagSlot, PropertyId::ActionSlot })
            S.MarkValid(P);
    }
    {
        auto& S = Schemas[HktType::Building];
        S.TypeId = HktType::Building;
        for (uint16 P : {
            PropertyId::PosX, PropertyId::PosY, PropertyId::PosZ,
            PropertyId::TargetPosX, PropertyId::TargetPosY, PropertyId::TargetPosZ, PropertyId::Param0, PropertyId::Param1,
            PropertyId::OwnerEntity, PropertyId::EntityType, PropertyId::EntitySpawnTag,
            PropertyId::Health, PropertyId::MaxHealth,
            PropertyId::Team })
            S.MarkValid(P);
    }
}

FHktSchemaRegistry& FHktSchemaRegistry::Get()
{
    struct FInitHelper
    {
        FHktSchemaRegistry Registry;
        FInitHelper() { Registry.Initialize(); }
    };
    static FInitHelper Helper;
    return Helper.Registry;
}

// ============================================================================
// FHktEntityPool
// ============================================================================

void FHktEntityPool::Initialize(int32 ReserveCount)
{
    Data.Reserve(ReserveCount * Stride);
    SlotToEntity.Reserve(ReserveCount);
    TagContainers.Reserve(ReserveCount);
    OwnerUids.Reserve(ReserveCount);
}

int32 FHktEntityPool::AllocateSlot(FHktEntityId EntityId)
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

void FHktEntityPool::FreeSlot(int32 Slot)
{
    SlotToEntity[Slot] = InvalidEntityId;
    TagContainers[Slot].Reset();
    OwnerUids[Slot] = 0;
    FreeSlots.Add(Slot);
    ActiveCount--;
}

// ============================================================================
// FHktWorldState
// ============================================================================

void FHktWorldState::Initialize()
{
    EntitySlots.Reserve(HktLimits::MaxEntities);
    ActiveEvents.Reserve(HktLimits::MaxActiveEvents);
    Pool.Initialize(2176);  // 512 + 1024 + 512 + 128
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
    int32 Slot = Pool.AllocateSlot(NewId);
    EntitySlots[NewId] = Slot;
    Pool.Set(Slot, PropertyId::EntityType, TypeId);
    return NewId;
}

void FHktWorldState::RemoveEntity(FHktEntityId Id)
{
    if (!IsValidEntity(Id)) return;
    Pool.FreeSlot(EntitySlots[Id]);
    EntitySlots[Id] = -1;
}

int32 FHktWorldState::GetEntityCount() const
{
    return Pool.ActiveCount;
}

FHktEntityState FHktWorldState::ExtractEntityState(FHktEntityId Id) const
{
    FHktEntityState S;
    S.EntityId = Id;
    if (!IsValidEntity(Id)) return S;
    int32 Slot = EntitySlots[Id];
    S.TypeId = static_cast<FHktTypeId>(Pool.Get(Slot, PropertyId::EntityType));
    S.Data.SetNumUninitialized(FHktEntityPool::Stride);
    FMemory::Memcpy(S.Data.GetData(), Pool.EntityData(Slot), FHktEntityPool::Stride * sizeof(int32));
    S.Tags = Pool.TagContainers[Slot];
    S.OwnerUid = Pool.OwnerUids[Slot];
    return S;
}

FHktEntityId FHktWorldState::ImportEntityState(const FHktEntityState& InState)
{
    FHktEntityId Id = AllocateEntity(InState.TypeId);
    int32 Slot = EntitySlots[Id];
    int32 N = FMath::Min(FHktEntityPool::Stride, InState.Data.Num());
    FMemory::Memcpy(Pool.EntityData(Slot), InState.Data.GetData(), N * sizeof(int32));
    Pool.TagContainers[Slot] = InState.Tags;
    Pool.OwnerUids[Slot] = InState.OwnerUid;
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
    int32 Slot = Pool.AllocateSlot(Id);
    EntitySlots[Id] = Slot;
    int32 N = FMath::Min(FHktEntityPool::Stride, InState.Data.Num());
    FMemory::Memcpy(Pool.EntityData(Slot), InState.Data.GetData(), N * sizeof(int32));
    Pool.TagContainers[Slot] = InState.Tags;
    Pool.OwnerUids[Slot] = InState.OwnerUid;
}

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
        Pool.TagContainers[EntitySlots[D.EntityId]] = D.OldTags;
    }

    FrameNumber = Diff.FrameNumber - 1;
}

void FHktWorldState::CopyFrom(const FHktWorldState& Other)
{
    FrameNumber = Other.FrameNumber;
    RandomSeed = Other.RandomSeed;
    NextEntityId = Other.NextEntityId;
    EntitySlots = Other.EntitySlots;
    ActiveEvents = Other.ActiveEvents;

    Pool.Data = Other.Pool.Data;
    Pool.SlotToEntity = Other.Pool.SlotToEntity;
    Pool.FreeSlots = Other.Pool.FreeSlots;
    Pool.ActiveCount = Other.Pool.ActiveCount;
    Pool.TagContainers = Other.Pool.TagContainers;
    Pool.OwnerUids = Other.Pool.OwnerUids;
}

// ============================================================================
// Serialization
// ============================================================================

bool FHktWorldState::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
    Ar << FrameNumber << RandomSeed << NextEntityId;

    if (Ar.IsSaving())
    {
        int32 TotalEntities = Pool.ActiveCount;
        Ar << TotalEntities;

        Pool.ForEachEntity([&](FHktEntityId Id, int32 Slot)
        {
            Ar << Id;
            uint8 TypeByte = static_cast<uint8>(Pool.Get(Slot, PropertyId::EntityType));
            Ar << TypeByte;

            for (int32 P = 0; P < FHktEntityPool::Stride; ++P)
            {
                int32 Val = Pool.EntityData(Slot)[P];
                Ar << Val;
            }

            Pool.TagContainers[Slot].NetSerialize(Ar, Map, bOutSuccess);
            Ar << Pool.OwnerUids[Slot];
        });
    }
    else // IsLoading
    {
        EntitySlots.Reset();
        Pool.Data.Reset();
        Pool.SlotToEntity.Reset();
        Pool.FreeSlots.Reset();
        Pool.TagContainers.Reset();
        Pool.OwnerUids.Reset();
        Pool.ActiveCount = 0;

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

            int32 Slot = Pool.AllocateSlot(Id);
            EntitySlots[Id] = Slot;

            for (int32 P = 0; P < FHktEntityPool::Stride; ++P)
                Ar << Pool.EntityData(Slot)[P];

            Pool.TagContainers[Slot].NetSerialize(Ar, Map, bOutSuccess);
            Ar << Pool.OwnerUids[Slot];
        }
    }

    Ar << ActiveEvents;
    return true;
}

// ============================================================================
// FHktWorldState — Tag Access
// ============================================================================

const FGameplayTagContainer& FHktWorldState::GetTags(FHktEntityId Entity) const
{
    static FGameplayTagContainer Empty;
    if (!IsValidEntity(Entity)) return Empty;
    return Pool.GetTags(EntitySlots[Entity]);
}

void FHktWorldState::AddTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity)) return;
    Pool.AddTag(EntitySlots[Entity], Tag);
}

void FHktWorldState::RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity)) return;
    Pool.RemoveTag(EntitySlots[Entity], Tag);
}

bool FHktWorldState::HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    if (!IsValidEntity(Entity)) return false;
    return Pool.HasTag(EntitySlots[Entity], Tag);
}
