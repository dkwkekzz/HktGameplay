// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * PropertyId - 엔티티 속성 ID
 *
 * Dense enum: HKT_PROPERTY(Name) 매크로 하나로 인덱스+이름 자동 생성.
 * PropertyId 자체가 SOA 배열 오프셋 — LocalIndex 매핑 불필요.
 */

#define HKT_PROPERTY_LIST(X) \
    /* 위치/이동 */           \
    X(PosX)                   \
    X(PosY)                   \
    X(PosZ)                   \
    X(RotYaw)                 \
    X(MoveTargetX)            \
    X(MoveTargetY)            \
    X(MoveTargetZ)            \
    X(MoveForce)              \
    X(IsMoving)               \
    X(MaxSpeed)               \
    /* 전투/상태 */           \
    X(Health)                 \
    X(MaxHealth)              \
    X(AttackPower)            \
    X(Defense)                \
    X(Team)                   \
    X(Mana)                   \
    X(MaxMana)                \
    /* 소유/타입 */           \
    X(OwnerEntity)            \
    X(EntityType)             \
    X(EntitySpawnTag)         \
    /* 이벤트 파라미터 */     \
    X(TargetPosX)             \
    X(TargetPosY)             \
    X(TargetPosZ)             \
    X(Param0)                 \
    X(Param1)                 \
    X(Param2)                 \
    X(Param3)                 \
    /* 애니메이션/비주얼 */   \
    X(AnimState)              \
    X(VisualState)            \
    X(AnimStateUpper)         \
    /* 물리 */                \
    X(VelX)                   \
    X(VelY)                   \
    X(VelZ)                   \
    X(Mass)                   \
    X(CollisionRadius)        \
    /* 아이템 */              \
    X(ItemState)              \
    X(ItemId)                 \
    X(BagSlot)                \
    X(ActionSlot)             \
    /* NPC */                 \
    X(IsNPC)                  \
    X(SpawnFlowTag)

namespace PropertyId
{
    enum : uint16
    {
        #define HKT_PROP_ENUM(Name) Name,
        HKT_PROPERTY_LIST(HKT_PROP_ENUM)
        #undef HKT_PROP_ENUM
        MaxCount
    };
}

/** PropertyId -> 이름 문자열 (디버그/인사이트용) */
inline const TCHAR* GetPropertyName(uint16 PropId)
{
    switch (PropId)
    {
    #define HKT_PROP_NAME(Name) case PropertyId::Name: return TEXT(#Name);
    HKT_PROPERTY_LIST(HKT_PROP_NAME)
    #undef HKT_PROP_NAME
    default: return nullptr;
    }
}
