// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryMoveTo
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_MoveTo, "Action.Move.ToLocation", "Move to location action flow.");

	// State Tags — AnimInstance가 태그를 보고 애니메이션을 자동 재생
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Locomotion_Run, "Anim.FullBody.Locomotion.Run", "Run locomotion state tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Locomotion_Idle, "Anim.FullBody.Locomotion.Idle", "Idle state tag.");

	// VFX (클라이언트 즉시 재생 — PresentationSubsystem::OnIntentSubmitted에서 처리)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_MoveIndicator, "VFX.MoveIndicator", "Move destination indicator VFX.");

	/**
	 * ================================================================
	 * 위치 이동 Flow
	 *
	 * 자연어로 읽으면:
	 * "이동 상태 태그를 추가하면 AnimInstance가 자동으로 이동 애니메이션을 재생한다.
	 *  목표 위치로 이동하고, 도착하면 정지 상태 태그로 전환한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_MoveTo)
			.CancelOnDuplicate()
			.Log(TEXT("MoveTo: 이동 시작"))

			// 목표 위치 로드 (IntentEvent에서 설정됨)
			.LoadStore(R0, PropertyId::TargetPosX)
			.LoadStore(R1, PropertyId::TargetPosY)
			.LoadStore(R2, PropertyId::TargetPosZ)

			// 이동 상태 태그 추가 → AnimInstance가 태그를 감지하여 이동 애니메이션 자동 재생
			.RemoveTag(Self, Tag_Anim_FullBody_Locomotion_Idle)
			.AddTag(Self, Tag_Anim_FullBody_Locomotion_Run)

			// 목표 위치로 이동 시작 (힘 1500, Mass=100일 때 가속도 15cm/s²)
			.MoveToward(Self, R0, 150)

			// 이동 완료 대기
			.WaitMoveEnd(Self)

			// 정지 — 상태 태그 전환
			.StopMovement(Self)
			.RemoveTag(Self, Tag_Anim_FullBody_Locomotion_Run)
			.AddTag(Self, Tag_Anim_FullBody_Locomotion_Idle)

			.Log(TEXT("MoveTo: 도착"))
			.Halt()
			.BuildAndRegister();
	}
}
