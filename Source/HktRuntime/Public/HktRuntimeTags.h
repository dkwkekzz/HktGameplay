#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace HktGameplayTags
{
    // Story IDs - HktStoryDefinitions.h에서 사용되는 Story 식별자
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_Fireball);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Move_ToLocation);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Character_Spawn);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Basic);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_Heal);

    // Effect Tags
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Burn);

    // Animation Parent Tags — 태그 계층 루트
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Anim_FullBody);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Anim_UpperBody);
}
