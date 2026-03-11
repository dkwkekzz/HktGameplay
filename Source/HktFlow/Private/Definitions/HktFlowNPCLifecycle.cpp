// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktCoreProperties.h"
#include "HktFlowRegistry.h"
#include "NativeGameplayTags.h"

namespace HktFlowNPCLifecycle
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_NPC_Lifecycle, "Flow.NPC.Lifecycle", "NPC lifecycle management (death/despawn).");

	// Anim
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Death, "Anim.Death", "Death animation.");

	/**
	 * ================================================================
	 * NPC 생명주기 Flow
	 *
	 * 자연어로 읽으면:
	 * "1초마다 체력을 확인한다.
	 *  체력이 0 이하이면 죽음 애니메이션을 재생하고
	 *  3초 후 엔티티를 제거한다."
	 *
	 * NPC 스폰 시 함께 fire되어야 함.
	 * Self = NPC 엔티티
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;

		Flow(Flow_NPC_Lifecycle)
			.Label(TEXT("check"))
				.LoadEntityProperty(R0, Self, PropertyId::Health)
				.LoadConst(R1, 0)
				.CmpLe(Flag, R0, R1)                    // Health <= 0?
				.JumpIf(Flag, TEXT("die"))
				.WaitSeconds(1.0f)
				.Jump(TEXT("check"))

			.Label(TEXT("die"))
				.Log(TEXT("NPC died"))
				.PlayAnim(Self, Anim_Death)
				.WaitSeconds(3.0f)
				.DestroyEntity(Self)
				.Halt()
			.BuildAndRegister();
	}
}
