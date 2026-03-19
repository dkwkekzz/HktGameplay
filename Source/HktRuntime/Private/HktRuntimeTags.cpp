#include "HktRuntimeTags.h"

namespace HktGameplayTags
{
    // Story IDs - Story 식별자 (Story.{호출형태}.{카테고리}.{이름})
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Event_Skill_Fireball, "Story.Event.Skill.Fireball", "Fireball skill: projectile with direct hit and AoE burn damage.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Event_Move_ToLocation, "Story.Event.Move.ToLocation", "Move to target location and stop on arrival.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Flow_Character_Spawn, "Story.Flow.Character.Spawn", "Character spawn flow: spawn character with items and intro animation.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Event_Attack_Basic, "Story.Event.Attack.Basic", "Basic attack: deal damage based on attack power.");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Event_Skill_Heal, "Story.Event.Skill.Heal", "Heal skill: restore health up to max health.");

    // Effect Tags
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Burn, "Effect.Burn", "Burn effect: fire damage over time.");

    // Animation Layer Parent Tags — 태그 계층의 레이어 루트
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_FullBody, "Anim.FullBody", "Full body animation layer root (locomotion, idle, death).");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_UpperBody, "Anim.UpperBody", "Upper body animation layer root (attack, cast override).");
}
