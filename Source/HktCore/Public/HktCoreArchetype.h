// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreProperties.h"

// ============================================================================
// EHktArchetype — 엔티티 유형 열거
// ============================================================================

enum class EHktArchetype : uint8
{
    None = 0,
    Character,
    NPC,
    Item,
    Projectile,
    Building,
    Max
};

// ============================================================================
// FHktPropertyTrait — 프로퍼티 특성 (Movable, Combatable, Collidable 등)
//
// 여러 Archetype이 공유하는 프로퍼티 그룹.
// 새 프로퍼티를 Trait에 추가하면 해당 Trait을 사용하는 모든 Archetype에 자동 반영.
// ============================================================================

struct FHktPropertyTrait
{
    FName Name;
    TArray<uint16> PropertyIds;
};

// ============================================================================
// FHktArchetypeMetadata — Archetype 메타데이터
// ============================================================================

struct FHktArchetypeMetadata
{
    EHktArchetype Type = EHktArchetype::None;
    const TCHAR* Name = TEXT("None");
    TArray<uint16> PropertyIds;     // 최종 병합된 프로퍼티 목록
    TArray<FName> TraitNames;       // 이 Archetype이 포함하는 Trait 이름들

    bool HasProperty(uint16 PropId) const
    {
        return PropertyIds.Contains(PropId);
    }

    bool HasTrait(FName TraitName) const
    {
        return TraitNames.Contains(TraitName);
    }

    const TCHAR* ToString() const { return Name; }
};

// ============================================================================
// FHktArchetypeRegistry — Archetype 레지스트리 (Meyers singleton)
// ============================================================================

class HKTCORE_API FHktArchetypeRegistry
{
public:
    static FHktArchetypeRegistry& Get();

    /** Trait 정의 — 프로퍼티 그룹에 이름 부여 */
    void DefineTrait(FName TraitName, std::initializer_list<uint16> Props);

    /** Archetype 등록 — Trait 조합 + 추가 프로퍼티 */
    void Register(EHktArchetype Type, const TCHAR* Name,
                  std::initializer_list<FName> Traits,
                  std::initializer_list<uint16> ExtraProps = {});

    /** ClassTag → Archetype 매핑 */
    void MapTag(const FGameplayTag& Tag, EHktArchetype Type);

    const FHktArchetypeMetadata* Find(EHktArchetype Type) const;
    EHktArchetype FindByTag(const FGameplayTag& Tag) const;
    EHktArchetype FindByName(const TCHAR* Name) const;
    const FHktPropertyTrait* FindTrait(FName TraitName) const;

private:
    FHktArchetypeMetadata Archetypes[static_cast<int>(EHktArchetype::Max)];
    TMap<FName, FHktPropertyTrait> Traits;
    TMap<FGameplayTag, EHktArchetype> TagMapping;
};

// ============================================================================
// Trait 이름 상수 — 오타 방지
// ============================================================================

namespace HktTrait
{
    inline const FName Spatial    = FName(TEXT("Spatial"));
    inline const FName Movable    = FName(TEXT("Movable"));
    inline const FName Collidable = FName(TEXT("Collidable"));
    inline const FName Combatable = FName(TEXT("Combatable"));
    inline const FName Animated   = FName(TEXT("Animated"));
    inline const FName EventParam = FName(TEXT("EventParam"));
    inline const FName Ownable    = FName(TEXT("Ownable"));
    inline const FName EquipSlots = FName(TEXT("EquipSlots"));

    /** EquipSlot0~8 PropertyId 배열 — Trait에서 가져옴 (5곳 중복 제거) */
    inline const TArray<uint16>& GetEquipSlotPropertyIds()
    {
        const FHktPropertyTrait* T = FHktArchetypeRegistry::Get().FindTrait(EquipSlots);
        check(T);
        return T->PropertyIds;
    }
}

// ============================================================================
// InitializeArchetypes — 모듈 시작 시 호출
// ============================================================================

HKTCORE_API void InitializeHktArchetypes();
