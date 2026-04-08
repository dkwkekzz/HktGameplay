// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreArchetype.h"

// ============================================================================
// FHktArchetypeRegistry
// ============================================================================

FHktArchetypeRegistry& FHktArchetypeRegistry::Get()
{
    static FHktArchetypeRegistry Instance;
    return Instance;
}

void FHktArchetypeRegistry::Register(EHktArchetype Type, const TCHAR* Name, std::initializer_list<uint16> Props)
{
    const int32 Idx = static_cast<int32>(Type);
    if (Idx <= 0 || Idx >= static_cast<int32>(EHktArchetype::Max)) return;

    FHktArchetypeMetadata& Meta = Archetypes[Idx];
    Meta.Type = Type;
    Meta.Name = Name;
    Meta.PropertyIds.Reset();
    Meta.PropertyIds.Append(Props.begin(), static_cast<int32>(Props.size()));
}

void FHktArchetypeRegistry::MapTag(const FGameplayTag& Tag, EHktArchetype Type)
{
    if (Tag.IsValid())
    {
        TagMapping.Add(Tag, Type);
    }
}

const FHktArchetypeMetadata* FHktArchetypeRegistry::Find(EHktArchetype Type) const
{
    const int32 Idx = static_cast<int32>(Type);
    if (Idx <= 0 || Idx >= static_cast<int32>(EHktArchetype::Max)) return nullptr;
    if (Archetypes[Idx].Type == EHktArchetype::None) return nullptr;
    return &Archetypes[Idx];
}

EHktArchetype FHktArchetypeRegistry::FindByTag(const FGameplayTag& Tag) const
{
    // 정확한 매칭 먼저
    if (const EHktArchetype* Found = TagMapping.Find(Tag))
    {
        return *Found;
    }

    // 부모 태그 매칭 (Entity.Character.Player → Entity.Character)
    for (const auto& Pair : TagMapping)
    {
        if (Tag.MatchesTag(Pair.Key))
        {
            return Pair.Value;
        }
    }

    return EHktArchetype::None;
}

// ============================================================================
// InitializeHktArchetypes
// ============================================================================

void InitializeHktArchetypes()
{
    auto& R = FHktArchetypeRegistry::Get();

    // ----- Character -----
    R.Register(EHktArchetype::Character, TEXT("Character"), {
        // Hot
        HktProperty::PosX, HktProperty::PosY, HktProperty::PosZ, HktProperty::RotYaw,
        HktProperty::MoveTargetX, HktProperty::MoveTargetY, HktProperty::MoveTargetZ,
        HktProperty::MoveForce, HktProperty::IsMoving, HktProperty::IsGrounded, HktProperty::MaxSpeed,
        HktProperty::Health, HktProperty::MaxHealth, HktProperty::AttackPower, HktProperty::Defense,
        HktProperty::Team, HktProperty::Mana, HktProperty::MaxMana,
        HktProperty::OwnerEntity, HktProperty::EntitySpawnTag,
        HktProperty::Stance,
        HktProperty::CP, HktProperty::MaxCP, HktProperty::AttackSpeed, HktProperty::MotionPlayRate,
        HktProperty::NextActionFrame,
        HktProperty::CollisionLayer, HktProperty::CollisionMask, HktProperty::CollisionRadius, HktProperty::Mass,
        // Cold
        HktProperty::TargetPosX, HktProperty::TargetPosY, HktProperty::TargetPosZ,
        HktProperty::Param0, HktProperty::Param1, HktProperty::Param2, HktProperty::Param3,
        HktProperty::AnimState, HktProperty::VisualState, HktProperty::AnimStateUpper,
        HktProperty::VelX, HktProperty::VelY, HktProperty::VelZ,
        HktProperty::BagCapacity,
        HktProperty::EquipSlot0, HktProperty::EquipSlot1, HktProperty::EquipSlot2,
        HktProperty::EquipSlot3, HktProperty::EquipSlot4, HktProperty::EquipSlot5,
        HktProperty::EquipSlot6, HktProperty::EquipSlot7, HktProperty::EquipSlot8,
        HktProperty::VoxelSkinSet, HktProperty::VoxelPalette,
    });

    // ----- NPC -----
    R.Register(EHktArchetype::NPC, TEXT("NPC"), {
        // Hot
        HktProperty::PosX, HktProperty::PosY, HktProperty::PosZ, HktProperty::RotYaw,
        HktProperty::MoveTargetX, HktProperty::MoveTargetY, HktProperty::MoveTargetZ,
        HktProperty::MoveForce, HktProperty::IsMoving, HktProperty::IsGrounded, HktProperty::MaxSpeed,
        HktProperty::Health, HktProperty::MaxHealth, HktProperty::AttackPower, HktProperty::Defense,
        HktProperty::Team,
        HktProperty::OwnerEntity, HktProperty::EntitySpawnTag,
        HktProperty::Stance,
        HktProperty::CP, HktProperty::MaxCP, HktProperty::AttackSpeed, HktProperty::MotionPlayRate,
        HktProperty::NextActionFrame,
        HktProperty::CollisionLayer, HktProperty::CollisionMask, HktProperty::CollisionRadius, HktProperty::Mass,
        // Cold
        HktProperty::TargetPosX, HktProperty::TargetPosY, HktProperty::TargetPosZ,
        HktProperty::Param0, HktProperty::Param1, HktProperty::Param2, HktProperty::Param3,
        HktProperty::AnimState, HktProperty::VisualState, HktProperty::AnimStateUpper,
        HktProperty::VelX, HktProperty::VelY, HktProperty::VelZ,
        HktProperty::IsNPC, HktProperty::SpawnFlowTag,
        HktProperty::VoxelSkinSet, HktProperty::VoxelPalette,
    });

    // ----- Item -----
    R.Register(EHktArchetype::Item, TEXT("Item"), {
        // Hot
        HktProperty::PosX, HktProperty::PosY, HktProperty::PosZ,
        HktProperty::OwnerEntity, HktProperty::EntitySpawnTag,
        HktProperty::CollisionLayer, HktProperty::CollisionMask, HktProperty::CollisionRadius, HktProperty::Mass,
        // Cold
        HktProperty::Param0, HktProperty::Param1, HktProperty::Param2, HktProperty::Param3,
        HktProperty::ItemState, HktProperty::ItemId, HktProperty::EquipIndex,
        HktProperty::Equippable,
        HktProperty::ItemSkillTag, HktProperty::SkillCPCost, HktProperty::RecoveryFrame,
        HktProperty::SkillTargetRequired, HktProperty::AttackRange, HktProperty::AttackPower,
    });

    // ----- Projectile -----
    R.Register(EHktArchetype::Projectile, TEXT("Projectile"), {
        // Hot
        HktProperty::PosX, HktProperty::PosY, HktProperty::PosZ, HktProperty::RotYaw,
        HktProperty::MoveTargetX, HktProperty::MoveTargetY, HktProperty::MoveTargetZ,
        HktProperty::MoveForce, HktProperty::IsMoving, HktProperty::MaxSpeed,
        HktProperty::AttackPower, HktProperty::Team,
        HktProperty::OwnerEntity, HktProperty::EntitySpawnTag,
        HktProperty::CollisionLayer, HktProperty::CollisionMask, HktProperty::CollisionRadius, HktProperty::Mass,
        // Cold
        HktProperty::VelX, HktProperty::VelY, HktProperty::VelZ,
    });

    // ----- Building -----
    R.Register(EHktArchetype::Building, TEXT("Building"), {
        HktProperty::PosX, HktProperty::PosY, HktProperty::PosZ, HktProperty::RotYaw,
        HktProperty::Health, HktProperty::MaxHealth,
        HktProperty::Team,
        HktProperty::OwnerEntity, HktProperty::EntitySpawnTag,
        HktProperty::CollisionLayer, HktProperty::CollisionMask, HktProperty::CollisionRadius, HktProperty::Mass,
    });

    // ----- ClassTag → Archetype 매핑 -----
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.Character")), EHktArchetype::Character);
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.NPC")),       EHktArchetype::NPC);
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.Item")),      EHktArchetype::Item);
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.Projectile")), EHktArchetype::Projectile);
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.Building")),  EHktArchetype::Building);
}
