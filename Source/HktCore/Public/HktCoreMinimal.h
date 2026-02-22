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
// ============================================================================

namespace HktType
{
    constexpr FHktTypeId None       = 0;
    constexpr FHktTypeId Unit       = 1;
    constexpr FHktTypeId Projectile = 2;
    constexpr FHktTypeId Equipment  = 3;
    constexpr FHktTypeId Building   = 4;
    constexpr FHktTypeId MaxTypes   = 8;
}
