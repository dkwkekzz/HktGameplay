// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// ============================================================================
// Entity ID
// ============================================================================

using FHktEntityId = int32;
constexpr FHktEntityId InvalidEntityId = -1;

// ============================================================================
// Stance (무기별 동작 모드) — FGameplayTag 기반
// ============================================================================

namespace HktStance
{
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unarmed);  // Stance.Unarmed
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Spear);    // Stance.Spear
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gun);      // Stance.Gun
	HKTCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sword1H);  // Stance.Sword1H
}
