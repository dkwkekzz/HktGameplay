// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * PropertyId - 엔티티 속성 ID (3-Tier Storage)
 *
 * Hot 프로퍼티: 직접 인덱싱 (O(1)) — 매 프레임 접근하는 핵심 속성
 * Cold 프로퍼티: {PropId, Value} 페어 배열 순회 — 공간 절약
 *
 * PropertyId 자체가 0..MaxCount-1 범위의 dense enum.
 * PropId < HotMaxCount 이면 Hot, 아니면 Cold.
 */

#define HKT_HOT_PROPERTY_LIST(X) \
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
    /* 소유 */                \
    X(OwnerEntity)            \
    X(EntitySpawnTag)         \
    /* 스탠스 */              \
    X(Stance)

#define HKT_COLD_PROPERTY_LIST(X) \
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

#define HKT_PROPERTY_LIST(X) \
    HKT_HOT_PROPERTY_LIST(X) \
    HKT_COLD_PROPERTY_LIST(X)

namespace PropertyId
{
    enum : uint16
    {
        #define HKT_PROP_ENUM(Name) Name,
        HKT_HOT_PROPERTY_LIST(HKT_PROP_ENUM)
        #undef HKT_PROP_ENUM
        HotMaxCount,

        #define HKT_PROP_ENUM(Name) Name,
        HKT_COLD_PROPERTY_LIST(HKT_PROP_ENUM)
        #undef HKT_PROP_ENUM
        MaxCount
    };

    // Cold enum의 시작 오프셋 (HotMaxCount 바로 다음)
    // 첫 번째 Cold 프로퍼티 ID == HotMaxCount
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
