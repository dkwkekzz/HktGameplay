// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ============================================================================
// Entity ID
// ============================================================================

using FHktEntityId = int32;
constexpr FHktEntityId InvalidEntityId = -1;

// ============================================================================
// Stance (무기별 동작 모드)
// ============================================================================

namespace HktStance
{
	constexpr int32 Invalid = 0;
	constexpr int32 Unarmed = 1;
	constexpr int32 Spear   = 2;
	constexpr int32 Gun     = 3;
	constexpr int32 Sword1H = 4;
	constexpr int32 Max     = 5;
}
