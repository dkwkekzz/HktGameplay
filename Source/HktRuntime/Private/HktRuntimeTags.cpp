#include "HktRuntimeTags.h"

namespace HktGameplayTags
{
    // --- Story IDs used by Runtime ---
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Event_Move_ToLocation, "Story.Event.Move.ToLocation", "Move to target location and stop on arrival.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_PlayerInWorld, "Story.State.Player.InWorld", "Player in world state flow.");

    // --- Database Defaults ---
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Visual_Character_Default, "Visual.Character.Default", "Default character visual.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_Character_Default, "Flow.Character.Default", "Default character flow.");

    // --- Entity Classification ---
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Character, "Entity.Character", "Player character entity root tag.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_NPC, "Entity.NPC", "Generic NPC tag.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Building, "Entity.Building", "Building entity.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Projectile, "Entity.Projectile", "Projectile entity.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item, "Entity.Item", "Item entity parent tag.");

    // --- Animation Layer Parent Tags ---
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim, "Anim", "Animation root tag.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_FullBody, "Anim.FullBody", "Full body animation layer root (locomotion, idle, death).");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Montage, "Anim.Montage", "Montage animation layer root.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_UpperBody, "Anim.UpperBody", "Upper body animation layer root (attack, cast override).");

    // --- Skill Slot Availability ---
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled, "Skill.Slot.Disabled", "Skill slot disabled parent tag.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_0, "Skill.Slot.Disabled.0", "Disable skill slot 0.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_1, "Skill.Slot.Disabled.1", "Disable skill slot 1.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_2, "Skill.Slot.Disabled.2", "Disable skill slot 2.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_3, "Skill.Slot.Disabled.3", "Disable skill slot 3.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_4, "Skill.Slot.Disabled.4", "Disable skill slot 4.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_5, "Skill.Slot.Disabled.5", "Disable skill slot 5.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_6, "Skill.Slot.Disabled.6", "Disable skill slot 6.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_7, "Skill.Slot.Disabled.7", "Disable skill slot 7.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Skill_Slot_Disabled_8, "Skill.Slot.Disabled.8", "Disable skill slot 8.");
}
