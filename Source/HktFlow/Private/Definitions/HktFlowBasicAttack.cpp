// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktCoreProperties.h"
#include "HktFlowRegistry.h"
#include "HktRuntimeTags.h"
#include "NativeGameplayTags.h"

namespace HktFlowBasicAttack
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_BasicAttack, "Ability.Attack.Basic", "Basic attack ability flow.");

	// Anim
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Attack, "Anim.Attack", "Basic attack upper body animation.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.HitSpark", "Melee hit spark VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/**
	 * ================================================================
	 * 추가 예제: 기본 공격 Flow
	 *
	 * 자연어로 읽으면:
	 * "상체 레이어에 공격 애니메이션을 재생하고 (하체는 이동 유지),
	 *  애니메이션이 끝나면 대상에게 공격력만큼 피해를 준다.
	 *  완료 후 상체 레이어를 초기화한다."
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;
		using namespace HktGameplayTags;

		Flow(Flow_BasicAttack)
			.Log(TEXT("BasicAttack: 공격 시작"))

			// 타겟 로드 (IntentEvent에서)
			.LoadStore(Target, PropertyId::Param0)      // Param0 = 타겟 EntityId

			// 상체 레이어에 공격 애니메이션 (하체 이동 유지)
			.PlayAnimLayer(Self, Anim_Layer_UpperBody, Anim_Attack)
			.PlayAnimMontage(Self, Anim_Montage_Attack)
			.WaitAnimEnd(Self)

			// 공격력 로드
			.LoadStore(R0, PropertyId::AttackPower)

			// 피해 적용
			.ApplyDamage(Target, R0)
			.PlayVFXAttached(Target, VFX_HitSpark)
			.PlaySound(Sound_Hit)

			// 상체 레이어 초기화
			.StopAnimLayer(Self, Anim_Layer_UpperBody)

			.Log(TEXT("BasicAttack: 완료"))
			.Halt()
			.BuildAndRegister();
	}
}
