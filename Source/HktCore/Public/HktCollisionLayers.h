// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Collision Layer Bitmask - 엔티티 충돌 레이어 정의
 *
 * CollisionLayer: 엔티티가 속하는 레이어 (단일 비트)
 * CollisionMask:  엔티티가 충돌할 대상 레이어 (복수 비트)
 *
 * 충돌 조건: (A.Layer & B.Mask) != 0 && (B.Layer & A.Mask) != 0
 */
namespace EHktCollisionLayer
{
    enum : uint32
    {
        None       = 0,
        Character  = 1 << 0,
        NPC        = 1 << 1,
        Projectile = 1 << 2,
        Building   = 1 << 3,
        Item       = 1 << 4,
        Trigger    = 1 << 5,

        // 편의 조합
        AllUnits   = Character | NPC,
        All        = 0xFFFFFFFF,
    };
}

/**
 * 엔티티 ClassTag에 따른 기본 Collision Layer 반환.
 * Tag 이름 계층으로 판단 (Entity.Character, Entity.NPC 등).
 */
inline uint32 GetDefaultCollisionLayer(const FGameplayTag& ClassTag)
{
    if (!ClassTag.IsValid())
        return EHktCollisionLayer::None;

    const FName TagName = ClassTag.GetTagName();
    if (TagName.ToString().StartsWith(TEXT("Entity.Character")))
        return EHktCollisionLayer::Character;
    if (TagName.ToString().StartsWith(TEXT("Entity.NPC")))
        return EHktCollisionLayer::NPC;
    if (TagName.ToString().StartsWith(TEXT("Entity.Projectile")))
        return EHktCollisionLayer::Projectile;
    if (TagName.ToString().StartsWith(TEXT("Entity.Building")))
        return EHktCollisionLayer::Building;
    if (TagName.ToString().StartsWith(TEXT("Entity.Item")))
        return EHktCollisionLayer::Item;

    return EHktCollisionLayer::None;
}

/**
 * 엔티티 ClassTag에 따른 기본 Collision Mask 반환.
 */
inline uint32 GetDefaultCollisionMask(const FGameplayTag& ClassTag)
{
    if (!ClassTag.IsValid())
        return EHktCollisionLayer::None;

    const FName TagName = ClassTag.GetTagName();
    if (TagName.ToString().StartsWith(TEXT("Entity.Character")))
        return EHktCollisionLayer::Character | EHktCollisionLayer::NPC | EHktCollisionLayer::Projectile | EHktCollisionLayer::Building;
    if (TagName.ToString().StartsWith(TEXT("Entity.NPC")))
        return EHktCollisionLayer::Character | EHktCollisionLayer::NPC | EHktCollisionLayer::Projectile | EHktCollisionLayer::Building;
    if (TagName.ToString().StartsWith(TEXT("Entity.Projectile")))
        return EHktCollisionLayer::Character | EHktCollisionLayer::NPC | EHktCollisionLayer::Building;
    if (TagName.ToString().StartsWith(TEXT("Entity.Building")))
        return EHktCollisionLayer::Character | EHktCollisionLayer::NPC | EHktCollisionLayer::Projectile;
    if (TagName.ToString().StartsWith(TEXT("Entity.Item")))
        return EHktCollisionLayer::None;

    return EHktCollisionLayer::None;
}
