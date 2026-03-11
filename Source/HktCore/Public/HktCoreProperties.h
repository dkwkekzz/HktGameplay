// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * PropertyId - 엔티티 속성 ID 상수
 *
 * SOA 레이아웃에서 각 속성의 인덱스를 정의
 * WorldState, VM Store 등에서 공통 사용
 *
 * X-매크로 패턴: ID 상수와 이름 문자열을 한 곳에서 관리
 */

#define HKT_PROPERTY_LIST(X) \
    /* 위치/이동 */           \
    X(PosX, 0)               \
    X(PosY, 1)               \
    X(PosZ, 2)               \
    X(RotYaw, 3)             \
    X(MoveTargetX, 4)        \
    X(MoveTargetY, 5)        \
    X(MoveTargetZ, 6)        \
    X(MoveForce, 7)          \
    X(IsMoving, 8)           \
    X(MaxSpeed, 9)           \
    /* 전투/상태 */           \
    X(Health, 10)            \
    X(MaxHealth, 11)         \
    X(AttackPower, 12)       \
    X(Defense, 13)           \
    X(Team, 14)              \
    X(Mana, 15)              \
    X(MaxMana, 16)           \
    /* 소유/타입 */           \
    X(OwnerEntity, 20)       \
    X(EntityType, 21)        \
    X(EntitySpawnTag, 22)    \
    /* 이벤트 파라미터 */     \
    X(TargetPosX, 30)        \
    X(TargetPosY, 31)        \
    X(TargetPosZ, 32)        \
    X(Param0, 33)            \
    X(Param1, 34)            \
    X(Param2, 35)            \
    X(Param3, 36)            \
    /* 애니메이션/비주얼 */   \
    X(AnimState, 40)         \
    X(VisualState, 41)       \
    /* 물리 */                \
    X(VelX, 50)              \
    X(VelY, 51)              \
    X(VelZ, 52)              \
    X(Mass, 53)              \
    X(CollisionRadius, 54)

namespace PropertyId
{
    #define HKT_PROPID_CONST(Name, Id) constexpr uint16 Name = Id;
    HKT_PROPERTY_LIST(HKT_PROPID_CONST)
    #undef HKT_PROPID_CONST
}

/** PropertyId → 이름 문자열 (디버그/인사이트용) */
inline const TCHAR* GetPropertyName(uint16 PropId)
{
    switch (PropId)
    {
    #define HKT_PROPID_NAME(Name, Id) case Id: return TEXT(#Name);
    HKT_PROPERTY_LIST(HKT_PROPID_NAME)
    #undef HKT_PROPID_NAME
    default: return nullptr;
    }
}
