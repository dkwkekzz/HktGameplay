// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryBasicAttack
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_BasicAttack, "Story.Event.Attack.Basic", "Basic attack ability flow.");

	// State Tags — AnimInstance가 태그를 보고 애니메이션을 자동 재생
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_UpperBody_Combat_Attack, "Anim.UpperBody.Combat.Attack", "Basic attack upper body state tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage state tag.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.Niagara.HitSpark", "Melee hit spark VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/**
	 * ================================================================
	 * 기본 공격 Flow
	 *
	 * 자연어로 읽으면:
	 * "공격 상태 태그를 추가하면 AnimInstance가 자동으로 애니메이션을 재생한다.
	 *  대상에게 공격력만큼 피해를 준다.
	 *  완료 후 공격 상태 태그를 제거한다."
	 * ================================================================
	 */
	/** CP 기본 공격 적중 시 회복량 */
	static constexpr int32 BasicAttackCPGain = 10;

	/** 기본 공격 후딜레이 (프레임) — AttackSpeed로 나뉨 */
	static constexpr int32 BasicAttackRecoveryFrame = 30;

	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_BasicAttack)
			.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity))
					return false;
				// NextActionFrame 검증 — 현재 프레임이 충분히 지났는지
				int32 NextFrame = WS.GetProperty(E.SourceEntity, PropertyId::NextActionFrame);
				return WS.FrameNumber >= NextFrame;
			})
			.Log(TEXT("BasicAttack: 공격 시작"))

			// === 공속 기반 쿨타임 검증 (서버 권위적 이중 검증) ===
			.GetWorldTime(R0)                                       // R0 = 현재 프레임
			.LoadStore(R1, PropertyId::NextActionFrame)             // R1 = NextActionFrame
			.CmpLt(Flag, R0, R1)                                    // 현재 < NextActionFrame?
			.JumpIf(Flag, TEXT("fail"))                             // 아직 쿨타임 중이면 실패

			// 타겟 로드 (IntentEvent에서)
			.LoadStore(Target, PropertyId::Param0)                  // Param0 = 타겟 EntityId

			// 공격 상태 태그 추가 → AnimInstance가 태그를 감지하여 애니메이션 자동 재생
			.AddTag(Self, Tag_Anim_UpperBody_Combat_Attack)
			.AddTag(Self, Tag_Anim_Montage_Attack)
			.WaitAnimEnd(Self)

			// 공격력 로드
			.LoadStore(R0, PropertyId::AttackPower)

			// 피해 적용
			.ApplyDamage(Target, R0)
			.PlayVFXAttached(Target, VFX_HitSpark)
			.PlaySound(Sound_Hit)

			// === CP 회복 (적중 시) ===
			.LoadStore(R0, PropertyId::CP)                          // R0 = 현재 CP
			.LoadStore(R1, PropertyId::MaxCP)                       // R1 = MaxCP
			.LoadConst(R2, BasicAttackCPGain)                       // R2 = 회복량
			.Add(R0, R0, R2)                                        // R0 = CP + 회복량
			.CmpGt(R3, R0, R1)                                      // CP > MaxCP?
			.JumpIfNot(R3, TEXT("NoCPClamp"))
			.Move(R0, R1)                                           // MaxCP로 제한
		.Label(TEXT("NoCPClamp"))
			.SaveStore(PropertyId::CP, R0)                          // CP 저장

			// === NextActionFrame 갱신 (공속 기반) ===
			// NextActionFrame = 현재프레임 + (RecoveryFrame * 100 / AttackSpeed)
			.GetWorldTime(R0)                                       // R0 = 현재 프레임
			.LoadConst(R1, BasicAttackRecoveryFrame)                // R1 = 기본 후딜레이
			.LoadConst(R2, 100)                                     // R2 = 100 (스케일 상수)
			.Mul(R1, R1, R2)                                        // R1 = RecoveryFrame * 100
			.LoadStore(R2, PropertyId::AttackSpeed)                 // R2 = AttackSpeed
			.Div(R1, R1, R2)                                        // R1 = RecoveryFrame * 100 / AttackSpeed
			.Add(R0, R0, R1)                                        // R0 = 현재프레임 + 딜레이
			.SaveStore(PropertyId::NextActionFrame, R0)             // NextActionFrame 저장

			// 공격 상태 태그 제거
			.RemoveTag(Self, Tag_Anim_UpperBody_Combat_Attack)
			.RemoveTag(Self, Tag_Anim_Montage_Attack)

			.Log(TEXT("BasicAttack: 완료"))
			.Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("BasicAttack: 쿨타임 중 — 실패"))
			.Fail()
		.BuildAndRegister();
	}
}
