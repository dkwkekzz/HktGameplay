// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryDeath
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Death, "Story.Event.Death", "Death story — death animation + fade + destroy.");

	// 사망 애니메이션
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Action_Death, "Anim.FullBody.Action.Death", "Death state tag.");

	/**
	 * ================================================================
	 * Death Story
	 *
	 * 자연어로 읽으면:
	 * "죽는 애니메이션을 재생하고, 3초 후 서서히 사라진다."
	 *
	 * Self = 공격자 (SourceEntity)
	 * Target = 사망 대상 (TargetEntity)
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_Death)
			.CancelOnDuplicate()
			.Log(TEXT("Death: 사망 처리 시작"))

			// === 1. 죽는 애니메이션 재생 ===
			.AddTag(Target, Tag_Anim_FullBody_Action_Death)

			// === 2. 페이드아웃 대기 ===
			.WaitSeconds(3.0f)

			// === 3. 엔티티 제거 ===
			.DestroyEntity(Target)

			.Log(TEXT("Death: 완료"))
			.Halt()
		.BuildAndRegister();
	}
}
