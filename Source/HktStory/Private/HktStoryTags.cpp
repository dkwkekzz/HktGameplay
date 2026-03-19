// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktStoryTags.h"

namespace HktStoryTags
{
	// --- Entity Filter ---
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Entity_Item,              "Entity.Item",              "Item entity parent tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Character_Player,      "Entity.Character.Player",  "Player character entity.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Entity_NPC,               "Entity.NPC",               "Generic NPC tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_NPC_Goblin,            "Entity.NPC.Goblin",        "Goblin NPC entity.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_NPC_Skeleton,          "Entity.NPC.Skeleton",      "Skeleton NPC entity.");

	// --- Entity Attr ---
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_NPC_Hostile,              "Entity.Attr.NPC.Hostile",   "Hostile NPC tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Item_Material,            "Entity.Attr.Item.Material", "Material item category.");

	// --- Anim ---
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Action_Spawn, "Anim.FullBody.Action.Spawn", "Spawn intro state tag.");

	// --- VFX (Niagara convention: VFX.Niagara.{Name}) ---
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_SpawnEffect,              "VFX.Niagara.SpawnEffect",  "Character spawn VFX.");

	// --- Sound ---
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Spawn,                  "Sound.Spawn",              "Character spawn sound.");
}
