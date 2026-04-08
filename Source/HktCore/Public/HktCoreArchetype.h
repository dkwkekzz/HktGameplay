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
// FHktArchetypeMetadata — Archetype 메타데이터
// ============================================================================

struct FHktArchetypeMetadata
{
    EHktArchetype Type = EHktArchetype::None;
    const TCHAR* Name = TEXT("None");
    TArray<uint16> PropertyIds;

    bool HasProperty(uint16 PropId) const
    {
        return PropertyIds.Contains(PropId);
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

    void Register(EHktArchetype Type, const TCHAR* Name, std::initializer_list<uint16> Props);
    void MapTag(const FGameplayTag& Tag, EHktArchetype Type);

    const FHktArchetypeMetadata* Find(EHktArchetype Type) const;
    EHktArchetype FindByTag(const FGameplayTag& Tag) const;

private:
    FHktArchetypeMetadata Archetypes[static_cast<int>(EHktArchetype::Max)];
    TMap<FGameplayTag, EHktArchetype> TagMapping;
};

// ============================================================================
// InitializeArchetypes — 모듈 시작 시 호출
// ============================================================================

HKTCORE_API void InitializeHktArchetypes();
