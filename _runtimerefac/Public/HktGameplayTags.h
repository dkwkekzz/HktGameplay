#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace HktGameplayTags
{
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Widget_LoginHud);

    // Flow IDs - HktFlowDefinitions.h에서 사용되는 Flow 식별자
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_Fireball);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action_Move_ToLocation);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Character_Spawn);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack_Basic);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Skill_Heal);

    // Effect Tags
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Burn);
}
