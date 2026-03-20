// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// ============================================================================
// HktStoryTags — Story 간 공유되는 GameplayTag 선언
//
// 여러 Story에서 사용되는 태그를 한 곳에서 관리한다.
// 각 Story 파일에서 중복 UE_DEFINE_GAMEPLAY_TAG_COMMENT 하는 대신 이 헤더를 include.
//
// Note: Entity 분류 태그(Entity.Character, Entity.NPC, Entity.Item 등)와
//       Story ID 태그(Story.Event.Move.ToLocation 등)는 HktRuntimeTags.h 참조.
// ============================================================================

namespace HktStoryTags
{
	// --- Entity Filter (Story-specific sub-tags) ---
	HKTSTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_Character_Player);      // Entity.Character.Player
	HKTSTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_NPC_Goblin);            // Entity.NPC.Goblin
	HKTSTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_NPC_Skeleton);          // Entity.NPC.Skeleton

	// --- Entity Attr ---
	HKTSTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_NPC_Hostile);              // Entity.Attr.NPC.Hostile
	HKTSTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Item_Material);            // Entity.Attr.Item.Material

	// --- Anim ---
	HKTSTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_Anim_FullBody_Action_Spawn); // Anim.FullBody.Action.Spawn

	// --- VFX (Niagara convention: VFX.Niagara.{Name}) ---
	HKTSTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(VFX_SpawnEffect);              // VFX.Niagara.SpawnEffect

	// --- Sound ---
	HKTSTORY_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sound_Spawn);                  // Sound.Spawn
}
