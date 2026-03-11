// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ============================================================================
// Entity ID
// ============================================================================

using FHktEntityId = int32;
constexpr FHktEntityId InvalidEntityId = -1;

using FHktTypeId = uint8;

// ============================================================================
// Entity Types
// X-매크로 패턴: ID 상수와 이름 문자열을 한 곳에서 관리
// ============================================================================

#define HKT_TYPE_LIST(X) \
    X(None, 0)           \
    X(Unit, 1)           \
    X(Projectile, 2)     \
    X(Equipment, 3)      \
    X(Building, 4)

namespace HktType
{
    #define HKT_TYPE_CONST(Name, Id) constexpr FHktTypeId Name = Id;
    HKT_TYPE_LIST(HKT_TYPE_CONST)
    #undef HKT_TYPE_CONST

    constexpr FHktTypeId MaxTypes = 8;
}

/** HktType → 이름 문자열 (디버그/인사이트용) */
inline const TCHAR* GetTypeName(FHktTypeId TypeId)
{
    switch (TypeId)
    {
    #define HKT_TYPE_NAME(Name, Id) case Id: return TEXT(#Name);
    HKT_TYPE_LIST(HKT_TYPE_NAME)
    #undef HKT_TYPE_NAME
    default: return nullptr;
    }
}
