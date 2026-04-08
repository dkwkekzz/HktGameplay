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

void FHktArchetypeRegistry::DefineTrait(FName TraitName, std::initializer_list<uint16> Props)
{
    FHktPropertyTrait& T = Traits.FindOrAdd(TraitName);
    T.Name = TraitName;
    T.PropertyIds.Reset();
    T.PropertyIds.Append(Props.begin(), static_cast<int32>(Props.size()));
}

void FHktArchetypeRegistry::Register(
    EHktArchetype Type, const TCHAR* Name,
    std::initializer_list<FName> TraitList,
    std::initializer_list<uint16> ExtraProps)
{
    const int32 Idx = static_cast<int32>(Type);
    if (Idx <= 0 || Idx >= static_cast<int32>(EHktArchetype::Max)) return;

    FHktArchetypeMetadata& Meta = Archetypes[Idx];
    Meta.Type = Type;
    Meta.Name = Name;
    Meta.PropertyIds.Reset();
    Meta.TraitNames.Reset();

    // Trait 프로퍼티 병합 (중복 제거)
    for (const FName& TName : TraitList)
    {
        Meta.TraitNames.Add(TName);
        if (const FHktPropertyTrait* T = Traits.Find(TName))
        {
            for (uint16 PropId : T->PropertyIds)
            {
                Meta.PropertyIds.AddUnique(PropId);
            }
        }
    }

    // 추가 프로퍼티 병합
    for (uint16 PropId : ExtraProps)
    {
        Meta.PropertyIds.AddUnique(PropId);
    }
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

const FHktPropertyTrait* FHktArchetypeRegistry::FindTrait(FName TraitName) const
{
    return Traits.Find(TraitName);
}

// ============================================================================
// InitializeHktArchetypes
// ============================================================================

void InitializeHktArchetypes()
{
    auto& R = FHktArchetypeRegistry::Get();

    // ===== Trait 정의 =====

    R.DefineTrait(HktTrait::Movable, {
        HktProperty::PosX, HktProperty::PosY, HktProperty::PosZ, HktProperty::RotYaw,
        HktProperty::MoveTargetX, HktProperty::MoveTargetY, HktProperty::MoveTargetZ,
        HktProperty::MoveForce, HktProperty::IsMoving, HktProperty::IsGrounded, HktProperty::MaxSpeed,
        HktProperty::VelX, HktProperty::VelY, HktProperty::VelZ,
    });

    R.DefineTrait(HktTrait::Collidable, {
        HktProperty::CollisionLayer, HktProperty::CollisionMask,
        HktProperty::CollisionRadius, HktProperty::Mass,
    });

    R.DefineTrait(HktTrait::Combatable, {
        HktProperty::Health, HktProperty::MaxHealth,
        HktProperty::AttackPower, HktProperty::Defense, HktProperty::Team,
        HktProperty::CP, HktProperty::MaxCP,
        HktProperty::AttackSpeed, HktProperty::MotionPlayRate,
        HktProperty::NextActionFrame, HktProperty::Stance,
    });

    R.DefineTrait(HktTrait::Animated, {
        HktProperty::AnimState, HktProperty::VisualState, HktProperty::AnimStateUpper,
        HktProperty::VoxelSkinSet, HktProperty::VoxelPalette,
    });

    R.DefineTrait(HktTrait::Ownable, {
        HktProperty::OwnerEntity, HktProperty::EntitySpawnTag,
    });

    R.DefineTrait(HktTrait::EquipSlots, {
        HktProperty::EquipSlot0, HktProperty::EquipSlot1, HktProperty::EquipSlot2,
        HktProperty::EquipSlot3, HktProperty::EquipSlot4, HktProperty::EquipSlot5,
        HktProperty::EquipSlot6, HktProperty::EquipSlot7, HktProperty::EquipSlot8,
    });

    R.DefineTrait(HktTrait::EventParam, {
        HktProperty::TargetPosX, HktProperty::TargetPosY, HktProperty::TargetPosZ,
        HktProperty::Param0, HktProperty::Param1, HktProperty::Param2, HktProperty::Param3,
    });

    // ===== Archetype 등록 (Trait 조합 + 고유 프로퍼티) =====

    R.Register(EHktArchetype::Character, TEXT("Character"),
        {HktTrait::Movable, HktTrait::Collidable, HktTrait::Combatable,
         HktTrait::Animated, HktTrait::Ownable, HktTrait::EventParam, HktTrait::EquipSlots},
        {
            HktProperty::Mana, HktProperty::MaxMana,
            HktProperty::BagCapacity,
        });

    R.Register(EHktArchetype::NPC, TEXT("NPC"),
        {HktTrait::Movable, HktTrait::Collidable, HktTrait::Combatable,
         HktTrait::Animated, HktTrait::Ownable, HktTrait::EventParam},
        {
            HktProperty::IsNPC, HktProperty::SpawnFlowTag,
        });

    R.Register(EHktArchetype::Item, TEXT("Item"),
        {HktTrait::Collidable, HktTrait::Ownable, HktTrait::EventParam},
        {
            HktProperty::PosX, HktProperty::PosY, HktProperty::PosZ,
            HktProperty::ItemState, HktProperty::ItemId, HktProperty::EquipIndex,
            HktProperty::Equippable,
            HktProperty::ItemSkillTag, HktProperty::SkillCPCost, HktProperty::RecoveryFrame,
            HktProperty::SkillTargetRequired, HktProperty::AttackRange,
            HktProperty::AttackPower, HktProperty::Defense, HktProperty::Stance,
        });

    R.Register(EHktArchetype::Projectile, TEXT("Projectile"),
        {HktTrait::Movable, HktTrait::Collidable, HktTrait::Ownable},
        {
            HktProperty::AttackPower, HktProperty::Team,
        });

    R.Register(EHktArchetype::Building, TEXT("Building"),
        {HktTrait::Collidable, HktTrait::Ownable},
        {
            HktProperty::PosX, HktProperty::PosY, HktProperty::PosZ, HktProperty::RotYaw,
            HktProperty::Health, HktProperty::MaxHealth, HktProperty::Team,
        });

    // ===== ClassTag → Archetype 매핑 =====

    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.Character")), EHktArchetype::Character);
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.NPC")),       EHktArchetype::NPC);
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.Item")),      EHktArchetype::Item);
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.Projectile")), EHktArchetype::Projectile);
    R.MapTag(FGameplayTag::RequestGameplayTag(FName("Entity.Building")),  EHktArchetype::Building);
}
