#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace HktGameplayTags
{
    // Story IDs - Story 식별자 (Story.{호출형태}.{카테고리}.{이름})
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Story_Event_Skill_Fireball);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Story_Event_Move_ToLocation);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Story_Flow_Character_Spawn);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Story_Event_Attack_Basic);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Story_Event_Skill_Heal);

    // Effect Tags
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Burn);

    // Animation Parent Tags — 태그 계층 루트
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Anim_FullBody);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Anim_UpperBody);
}
