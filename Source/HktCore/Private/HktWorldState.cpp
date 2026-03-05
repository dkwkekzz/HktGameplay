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
            PropertyId::AnimState, PropertyId::VisualState })
            S.AddProperty(P);
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
            S.AddProperty(P);
    }
    {
        auto& S = Schemas[HktType::Equipment];
        S.TypeId = HktType::Equipment;
        for (uint16 P : {
            PropertyId::OwnerEntity, PropertyId::AttackPower,
            PropertyId::TargetPosX, PropertyId::TargetPosY, PropertyId::TargetPosZ, PropertyId::Param0, PropertyId::Param1,
            PropertyId::OwnerEntity, PropertyId::EntityType, PropertyId::EntitySpawnTag,
            PropertyId::Defense })
            S.AddProperty(P);
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
            S.AddProperty(P);
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

void FHktEntityPool::Initialize(const FHktEntitySchema& InSchema, int32 ReserveCount)
{
    TypeId = InSchema.TypeId;
    Stride = InSchema.GetStride();
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
    const FHktSchemaRegistry& Reg = FHktSchemaRegistry::Get();
    EntityLocations.Reserve(HktLimits::MaxEntities);
    ActiveEvents.Reserve(HktLimits::MaxActiveEvents);

    Pools[HktType::Unit].Initialize(Reg.Get(HktType::Unit), 512);
    Pools[HktType::Projectile].Initialize(Reg.Get(HktType::Projectile), 1024);
    Pools[HktType::Equipment].Initialize(Reg.Get(HktType::Equipment), 512);
    Pools[HktType::Building].Initialize(Reg.Get(HktType::Building), 128);
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
    S.OwnerUid = P.OwnerUids[L.PoolSlot];
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
    P.OwnerUids[L.PoolSlot] = InState.OwnerUid;
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
    P.OwnerUids[Slot] = InState.OwnerUid;
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

    // 4. 프로퍼티 변경 되돌리기 (OldValue 복원)
    for (const FHktPropertyDelta& D : Diff.PropertyDeltas)
        SetProperty(D.EntityId, D.PropertyId, D.OldValue);

    // 5. 소유권 변경 되돌리기
    for (const FHktOwnerDelta& D : Diff.OwnerDeltas)
        SetOwnerUid(D.EntityId, D.OldOwnerUid);

    // 6. 태그 변경 되돌리기 (OldTags 복원)
    for (const FHktTagDelta& D : Diff.TagDeltas)
    {
        if (!IsValidEntity(D.EntityId)) continue;
        const FEntityLocation& L = EntityLocations[D.EntityId];
        Pools[L.TypeId].TagContainers[L.PoolSlot] = D.OldTags;
    }

    // 7. FrameNumber 복원
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
        FHktEntityPool& Dst = Pools[T];
        const FHktEntityPool& Src = Other.Pools[T];
        Dst.TypeId = Src.TypeId;
        Dst.Stride = Src.Stride;
        Dst.Data = Src.Data;
        Dst.SlotToEntity = Src.SlotToEntity;
        Dst.FreeSlots = Src.FreeSlots;
        Dst.ActiveCount = Src.ActiveCount;
        Dst.TagContainers = Src.TagContainers;
        Dst.OwnerUids = Src.OwnerUids;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool FHktWorldState::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
    Ar << FrameNumber << RandomSeed << NextEntityId;

    if (Ar.IsSaving())
    {
        // 활성 엔티티만 전송: (EntityId, TypeByte, PropertyData[], Tags)
        int32 TotalEntities = GetEntityCount();
        Ar << TotalEntities;

        for (int32 T = 1; T < HktType::MaxTypes; ++T)
        {
            FHktEntityPool& Pool = Pools[T];
            for (int32 Slot = 0; Slot < Pool.SlotToEntity.Num(); ++Slot)
            {
                FHktEntityId Id = Pool.SlotToEntity[Slot];
                if (Id == InvalidEntityId) continue;

                Ar << Id;
                uint8 TypeByte = static_cast<uint8>(T);
                Ar << TypeByte;

                for (int32 P = 0; P < Pool.Stride; ++P)
                {
                    int32 Val = Pool.EntityData(Slot)[P];
                    Ar << Val;
                }

                Pool.TagContainers[Slot].NetSerialize(Ar, Map, bOutSuccess);
                Ar << Pool.OwnerUids[Slot];
            }
        }
    }
    else // IsLoading
    {
        // 풀 초기화: TypeId/Stride만 싱글톤에서 복원
        EntityLocations.Reset();
        for (int32 T = 0; T < HktType::MaxTypes; ++T)
        {
            FHktEntityPool& Pool = Pools[T];
            Pool.Data.Reset();
            Pool.SlotToEntity.Reset();
            Pool.FreeSlots.Reset();
            Pool.TagContainers.Reset();
            Pool.OwnerUids.Reset();
            Pool.ActiveCount = 0;
            Pool.TypeId = static_cast<FHktTypeId>(T);
            Pool.Stride = (T > HktType::None)
                ? FHktSchemaRegistry::Get().Get(static_cast<FHktTypeId>(T)).GetStride()
                : 0;
        }

        int32 TotalEntities; Ar << TotalEntities;
        for (int32 i = 0; i < TotalEntities; ++i)
        {
            FHktEntityId Id; Ar << Id;
            uint8 TypeByte;  Ar << TypeByte;
            FHktTypeId TypeId = static_cast<FHktTypeId>(TypeByte);

            if (Id >= EntityLocations.Num())
            {
                int32 OldNum = EntityLocations.Num();
                EntityLocations.SetNum(Id + 1);
                for (int32 j = OldNum; j < EntityLocations.Num(); ++j)
                    EntityLocations[j] = { HktType::None, -1 };
            }

            FHktEntityPool& Pool = Pools[TypeId];
            int32 Slot = Pool.AllocateSlot(Id);
            EntityLocations[Id] = { TypeId, Slot };

            for (int32 P = 0; P < Pool.Stride; ++P)
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
