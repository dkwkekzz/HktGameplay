#include "HktGameplayTags.h"

namespace HktGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Widget_LoginHud, "Widget.LoginHud", "Login HUD widget.");

    // Flow IDs - HktFlowDefinitions.h에서 사용되는 Flow 식별자
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Skill_Fireball, "Ability.Skill.Fireball", "Fireball skill: projectile with direct hit and AoE burn damage.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Move_ToLocation, "Action.Move.ToLocation", "Move to target location and stop on arrival.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Character_Spawn, "Event.Character.Spawn", "Character spawn event: spawn character with equipment and intro animation.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Basic, "Ability.Attack.Basic", "Basic attack: deal damage based on attack power.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Skill_Heal, "Ability.Skill.Heal", "Heal skill: restore health up to max health.");

    // Effect Tags
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Burn, "Effect.Burn", "Burn effect: fire damage over time.");
}
