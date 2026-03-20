#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace HktGameplayTags
{
    // --- Story IDs used by Runtime ---
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Story_Event_Move_ToLocation);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Story_PlayerInWorld);

    // --- Database Defaults ---
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Visual_Character_Default);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Flow_Character_Default);

    // --- Entity Classification ---
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_Character);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_NPC);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_Building);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_Projectile);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Entity_Item);

    // --- Animation Parent Tags ---
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Anim);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Anim_FullBody);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Anim_Montage);
    HKTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Anim_UpperBody);
}
