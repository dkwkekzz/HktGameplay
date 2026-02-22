// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktWorldState.h"
#include "HktPropertyIds.h"
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
            PropertyId::MoveTargetX, PropertyId::MoveTargetY, PropertyId::MoveTargetZ,
            PropertyId::MoveSpeed, PropertyId::IsMoving,
            PropertyId::Health, PropertyId::MaxHealth,
            PropertyId::AttackPower, PropertyId::Defense,
            PropertyId::Team, PropertyId::Mana, PropertyId::MaxMana,
            PropertyId::OwnerEntity, PropertyId::OwnerPlayerHash,
            PropertyId::AnimState, PropertyId::VisualState })
            S.AddProperty(P);
    }
    {
        auto& S = Schemas[HktType::Projectile];
        S.TypeId = HktType::Projectile;
        for (uint16 P : {
            PropertyId::PosX, PropertyId::PosY, PropertyId::PosZ,
            PropertyId::MoveTargetX, PropertyId::MoveTargetY, PropertyId::MoveTargetZ,
            PropertyId::MoveSpeed, PropertyId::IsMoving,
            PropertyId::OwnerEntity, PropertyId::Team })
            S.AddProperty(P);
    }
    {
        auto& S = Schemas[HktType::Equipment];
        S.TypeId = HktType::Equipment;
        for (uint16 P : {
            PropertyId::OwnerEntity, PropertyId::AttackPower,
            PropertyId::Defense, PropertyId::OwnerPlayerHash })
            S.AddProperty(P);
    }
    {
        auto& S = Schemas[HktType::Building];
        S.TypeId = HktType::Building;
        for (uint16 P : {
            PropertyId::PosX, PropertyId::PosY, PropertyId::PosZ,
            PropertyId::Health, PropertyId::MaxHealth,
            PropertyId::Team, PropertyId::OwnerPlayerHash })
            S.AddProperty(P);
    }
}

// ============================================================================
// FHktEntityPool
// ============================================================================

void FHktEntityPool::Initialize(const FHktEntitySchema& InSchema, int32 ReserveCount)
{
    Schema = &InSchema;
    TypeId = InSchema.TypeId;
    Stride = InSchema.GetStride();
    Data.Reserve(ReserveCount * Stride);
    SlotToEntity.Reserve(ReserveCount);
    DirtyMask.Reserve(ReserveCount);
    DirtySlots.Reserve(256);
}

int32 FHktEntityPool::AllocateSlot(FHktEntityId EntityId)
{
    int32 Slot;
    if (FreeSlots.Num() > 0)
    {
        Slot = FreeSlots.Pop();
        SlotToEntity[Slot] = EntityId;
    }
    else
    {
        Slot = SlotToEntity.Num();
        SlotToEntity.Add(EntityId);
        Data.AddZeroed(Stride);
        DirtyMask.Add(0);
    }
    FMemory::Memzero(Data.GetData() + Slot * Stride, Stride * sizeof(int32));
    DirtyMask[Slot] = 0;
    ActiveCount++;
    return Slot;
}

void FHktEntityPool::FreeSlot(int32 Slot)
{
    SlotToEntity[Slot] = InvalidEntityId;
    FreeSlots.Add(Slot);
    ActiveCount--;
}

// ============================================================================
// FHktWorldState
// ============================================================================

void FHktWorldState::Initialize(const FHktSchemaRegistry& InRegistry)
{
    Registry = &InRegistry;
    EntityLocations.Reserve(HktLimits::MaxEntities);
    ActiveEvents.Reserve(HktLimits::MaxActiveEvents);

    Pools[HktType::Unit].Initialize(InRegistry.Get(HktType::Unit), 512);
    Pools[HktType::Projectile].Initialize(InRegistry.Get(HktType::Projectile), 1024);
    Pools[HktType::Equipment].Initialize(InRegistry.Get(HktType::Equipment), 512);
    Pools[HktType::Building].Initialize(InRegistry.Get(HktType::Building), 128);
}

FHktEntityId FHktWorldState::AllocateEntity(FHktTypeId TypeId)
{
    check(TypeId > HktType::None && TypeId < HktType::MaxTypes);
    FHktEntityId NewId = NextEntityId++;
    if (NewId >= EntityLocations.Num())
    {
        int32 OldNum = EntityLocations.Num();
        EntityLocations.SetNum(NewId + 1);
        for (int32 i = OldNum; i < EntityLocations.Num(); ++i)
            EntityLocations[i] = { HktType::None, -1 };
    }
    int32 Slot = Pools[TypeId].AllocateSlot(NewId);
    EntityLocations[NewId] = { TypeId, Slot };
    return NewId;
}

void FHktWorldState::RemoveEntity(FHktEntityId Id)
{
    if (!IsValidEntity(Id)) return;
    FEntityLocation& L = EntityLocations[Id];
    Pools[L.TypeId].FreeSlot(L.PoolSlot);
    L = { HktType::None, -1 };
}

void FHktWorldState::ResetDirtyIndices()
{
    for (int32 T = 1; T < HktType::MaxTypes; ++T)
        Pools[T].ResetDirty();
}

int32 FHktWorldState::GetEntityCount() const
{
    int32 N = 0;
    for (int32 T = 1; T < HktType::MaxTypes; ++T) N += Pools[T].ActiveCount;
    return N;
}

FHktEntityState FHktWorldState::ExtractEntityState(FHktEntityId Id) const
{
    FHktEntityState S;
    S.EntityId = Id;
    if (!IsValidEntity(Id)) return S;
    const FEntityLocation& L = EntityLocations[Id];
    S.TypeId = L.TypeId;
    const FHktEntityPool& P = Pools[L.TypeId];
    S.Data.SetNumUninitialized(P.Stride);
    FMemory::Memcpy(S.Data.GetData(), P.EntityData(L.PoolSlot), P.Stride * sizeof(int32));
    return S;
}

FHktEntityId FHktWorldState::ImportEntityState(const FHktEntityState& InState)
{
    FHktEntityId Id = AllocateEntity(InState.TypeId);
    const FEntityLocation& L = EntityLocations[Id];
    FHktEntityPool& P = Pools[L.TypeId];
    int32 N = FMath::Min(P.Stride, InState.Data.Num());
    FMemory::Memcpy(P.EntityData(L.PoolSlot), InState.Data.GetData(), N * sizeof(int32));
    return Id;
}

void FHktWorldState::CopyFrom(const FHktWorldState& Other)
{
    FrameNumber = Other.FrameNumber;
    RandomSeed = Other.RandomSeed;
    NextEntityId = Other.NextEntityId;
    EntityLocations = Other.EntityLocations;
    ActiveEvents = Other.ActiveEvents;
    for (int32 T = 0; T < HktType::MaxTypes; ++T)
    {
        const FHktEntityPool& Src = Other.Pools[T];
        FHktEntityPool& Dst = Pools[T];
        Dst.Schema = Src.Schema;
        Dst.TypeId = Src.TypeId;
        Dst.Stride = Src.Stride;
        Dst.Data = Src.Data;
        Dst.SlotToEntity = Src.SlotToEntity;
        Dst.FreeSlots = Src.FreeSlots;
        Dst.ActiveCount = Src.ActiveCount;
        Dst.DirtyMask = Src.DirtyMask;
        Dst.DirtySlots = Src.DirtySlots;
    }
    for (int32 T = 0; T < HktType::MaxTypes; ++T)
        Pools[T].Schema = &Registry->Get(T);
}

FArchive& operator<<(FArchive& Ar, FHktWorldState& State)
{
    Ar << State.FrameNumber << State.RandomSeed << State.NextEntityId;

    if (Ar.IsLoading())
    {
        int32 N; Ar << N;
        State.EntityLocations.SetNum(N);
        for (int32 i = 0; i < N; ++i)
        {
            Ar << State.EntityLocations[i].TypeId;
            Ar << State.EntityLocations[i].PoolSlot;
        }
    }
    else
    {
        int32 N = State.EntityLocations.Num(); Ar << N;
        for (auto& L : State.EntityLocations) { Ar << L.TypeId; Ar << L.PoolSlot; }
    }

    for (int32 T = 1; T < HktType::MaxTypes; ++T)
    {
        FHktEntityPool& P = State.Pools[T];
        Ar << P.Data << P.SlotToEntity << P.FreeSlots << P.ActiveCount;
    }

    Ar << State.ActiveEvents;
    return Ar;
}
