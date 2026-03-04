// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktCoreProperties.h"
#include "HktFlowRegistry.h"
#include "NativeGameplayTags.h"

namespace HktFlowMoveTo
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_MoveTo, "Action.Move.ToLocation", "Move to location action flow.");

	// Anim
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Run, "Anim.Run", "Run locomotion animation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Idle, "Anim.Idle", "Idle animation.");

	// VFX (클라이언트 즉시 재생 — PresentationSubsystem::OnIntentSubmitted에서 처리)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_MoveIndicator, "VFX.MoveIndicator", "Move destination indicator VFX.");

	/**
	 * ================================================================
	 * 위치 이동 Flow
	 *
	 * 자연어로 읽으면:
	 * "목표 위치로 이동하고, 도착하면 정지한다."
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;

		Flow(Flow_MoveTo)
			.CancelOnDuplicate()
			.Log(TEXT("MoveTo: 이동 시작"))

			// 목표 위치 로드 (IntentEvent에서 설정됨)
			.LoadStore(R0, PropertyId::TargetPosX)
			.LoadStore(R1, PropertyId::TargetPosY)
			.LoadStore(R2, PropertyId::TargetPosZ)

			// 이동 애니메이션 시작
			.PlayAnim(Self, Anim_Run)

			// 목표 위치로 이동 시작 (힘 1500, Mass=100일 때 가속도 15cm/s²)
			.MoveToward(Self, R0, 5000)

			// 이동 완료 대기
			.WaitMoveEnd(Self)

			// 정지
			.StopMovement(Self)
			.PlayAnim(Self, Anim_Idle)

			.Log(TEXT("MoveTo: 도착"))
			.Halt()
			.BuildAndRegister();
	}
}
