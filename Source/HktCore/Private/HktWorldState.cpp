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
            PropertyId::MoveTargetX, PropertyId::MoveTargetY, PropertyId::MoveTargetZ,
            PropertyId::MoveSpeed, PropertyId::IsMoving,
            PropertyId::Health, PropertyId::MaxHealth,
            PropertyId::AttackPower, PropertyId::Defense,
            PropertyId::Team, PropertyId::Mana, PropertyId::MaxMana,
            PropertyId::OwnerEntity, PropertyId::EntitySpawnTag, PropertyId::OwnedPlayerUid,
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
            PropertyId::Defense, PropertyId::OwnedPlayerUid })
            S.AddProperty(P);
    }
    {
        auto& S = Schemas[HktType::Building];
        S.TypeId = HktType::Building;
        for (uint16 P : {
            PropertyId::PosX, PropertyId::PosY, PropertyId::PosZ,
            PropertyId::Health, PropertyId::MaxHealth,
            PropertyId::Team, PropertyId::EntitySpawnTag, PropertyId::OwnedPlayerUid })
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
    TagContainers.Reserve(ReserveCount);
    TagsDirtyMask.Reserve(ReserveCount);
    TagsDirtySlots.Reserve(256);
    OldValueEntries.Reserve(256);
    OldTagEntries.Reserve(32);
}

int32 FHktEntityPool::AllocateSlot(FHktEntityId EntityId)
{
    int32 Slot;
    if (FreeSlots.Num() > 0)
    {
        Slot = FreeSlots.Pop();
        SlotToEntity[Slot] = EntityId;
        TagContainers[Slot].Reset();
        TagsDirtyMask[Slot] = 0;
    }
    else
    {
        Slot = SlotToEntity.Num();
        SlotToEntity.Add(EntityId);
        Data.AddZeroed(Stride);
        DirtyMask.Add(0);
        TagContainers.Add({});
        TagsDirtyMask.Add(0);
    }
    FMemory::Memzero(Data.GetData() + Slot * Stride, Stride * sizeof(int32));
    DirtyMask[Slot] = 0;
    ActiveCount++;
    return Slot;
}

void FHktEntityPool::FreeSlot(int32 Slot)
{
    SlotToEntity[Slot] = InvalidEntityId;
    TagContainers[Slot].Reset();
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
    int8 OwnerLP = Registry->Get(L.TypeId).GetLocalIndex(PropertyId::OwnedPlayerUid);
    if (OwnerLP != -1)
    {
        int64 OwnerUid = static_cast<int64>(Pools[L.TypeId].Get(L.PoolSlot, OwnerLP));
        Pools[L.TypeId].UntrackOwner(L.PoolSlot, OwnerUid);
    }
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
    S.Tags = P.TagContainers[L.PoolSlot];
    return S;
}

FHktEntityId FHktWorldState::ImportEntityState(const FHktEntityState& InState)
{
    FHktEntityId Id = AllocateEntity(InState.TypeId);
    const FEntityLocation& L = EntityLocations[Id];
    FHktEntityPool& P = Pools[L.TypeId];
    int32 N = FMath::Min(P.Stride, InState.Data.Num());
    FMemory::Memcpy(P.EntityData(L.PoolSlot), InState.Data.GetData(), N * sizeof(int32));
    P.TagContainers[L.PoolSlot] = InState.Tags;
    int8 OwnerLP = P.Schema->GetLocalIndex(PropertyId::OwnedPlayerUid);
    if (OwnerLP != -1)
    {
        int64 OwnerUid = static_cast<int64>(P.Get(L.PoolSlot, OwnerLP));
        P.TrackOwner(L.PoolSlot, 0, OwnerUid);
    }
    return Id;
}

void FHktWorldState::ImportEntityStateWithId(const FHktEntityState& InState)
{
    FHktEntityId Id = InState.EntityId;
    if (Id >= EntityLocations.Num())
    {
        int32 OldNum = EntityLocations.Num();
        EntityLocations.SetNum(Id + 1);
        for (int32 i = OldNum; i < EntityLocations.Num(); ++i)
            EntityLocations[i] = { HktType::None, -1 };
    }
    int32 Slot = Pools[InState.TypeId].AllocateSlot(Id);
    EntityLocations[Id] = { InState.TypeId, Slot };
    FHktEntityPool& P = Pools[InState.TypeId];
    int32 N = FMath::Min(P.Stride, InState.Data.Num());
    FMemory::Memcpy(P.EntityData(Slot), InState.Data.GetData(), N * sizeof(int32));
    P.TagContainers[Slot] = InState.Tags;
    int8 OwnerLP = P.Schema->GetLocalIndex(PropertyId::OwnedPlayerUid);
    if (OwnerLP != -1)
    {
        int64 OwnerUid = static_cast<int64>(P.Get(Slot, OwnerLP));
        P.TrackOwner(Slot, 0, OwnerUid);
    }
}

void FHktWorldState::UndoDiff(const FHktSimulationDiff& Diff)
{
    // 1. 스폰된 엔티티 제거 (스폰 취소)
    for (const FHktEntityState& S : Diff.SpawnedEntities)
        RemoveEntity(S.EntityId);

    // 2. NextEntityId 복원
    if (Diff.PrevNextEntityId != InvalidEntityId)
        NextEntityId = Diff.PrevNextEntityId;

    // 3. 제거된 엔티티 복원
    for (const FHktEntityState& S : Diff.RemovedEntityStates)
        ImportEntityStateWithId(S);

    // 4. 프로퍼티 변경 되돌리기 (OldValue 복원, Dirty 추적 없이)
    for (const FHktPropertyDelta& D : Diff.PropertyDeltas)
        SetPropertyRaw(D.EntityId, D.PropertyId, D.OldValue);

    // 5. 태그 변경 되돌리기 (OldTags 복원)
    for (const FHktTagDelta& D : Diff.TagDeltas)
    {
        if (!IsValidEntity(D.EntityId)) continue;
        const FEntityLocation& L = EntityLocations[D.EntityId];
        Pools[L.TypeId].TagContainers[L.PoolSlot] = D.OldTags;
    }

    // 6. FrameNumber 복원
    FrameNumber = Diff.FrameNumber - 1;
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
        Dst.TagContainers = Src.TagContainers;
        Dst.OwnerToSlots = Src.OwnerToSlots;
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
        Ar << P.Data << P.SlotToEntity << P.FreeSlots << P.ActiveCount << P.TagContainers;
    }

    Ar << State.ActiveEvents;
    return Ar;
}

// ============================================================================
// FHktWorldState — Tag Access
// ============================================================================

const FGameplayTagContainer& FHktWorldState::GetTags(FHktEntityId Entity) const
{
    static FGameplayTagContainer Empty;
    if (!IsValidEntity(Entity)) return Empty;
    const FEntityLocation& L = EntityLocations[Entity];
    return Pools[L.TypeId].GetTags(L.PoolSlot);
}

void FHktWorldState::AddTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity)) return;
    const FEntityLocation& L = EntityLocations[Entity];
    Pools[L.TypeId].AddTag(L.PoolSlot, Tag);
}

void FHktWorldState::RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    if (!IsValidEntity(Entity)) return;
    const FEntityLocation& L = EntityLocations[Entity];
    Pools[L.TypeId].RemoveTag(L.PoolSlot, Tag);
}

bool FHktWorldState::HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    if (!IsValidEntity(Entity)) return false;
    const FEntityLocation& L = EntityLocations[Entity];
    return Pools[L.TypeId].HasTag(L.PoolSlot, Tag);
}
