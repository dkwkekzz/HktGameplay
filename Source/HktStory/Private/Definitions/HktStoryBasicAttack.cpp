// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktWorldState.h"
#include "HktCoreEvents.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetCombat.h"

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

	/** CP 기본 공격 적중 시 회복량 */
	static constexpr int32 BasicAttackCPGain = 10;

	/** 기본 공격 후딜레이 (프레임) — AttackSpeed로 나뉨 */
	static constexpr int32 BasicAttackRecoveryFrame = 30;

	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_BasicAttack)
			.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity))
					return false;
				int32 NextFrame = WS.GetProperty(E.SourceEntity, PropertyId::NextActionFrame);
				return WS.FrameNumber >= NextFrame;
			})
			.Log(TEXT("BasicAttack: 공격 시작"));

		// === 공속 기반 쿨타임 검증 (서버 권위적 이중 검증) ===
		HktSnippetCombat::CooldownCheck(B, TEXT("fail"));

		B	// 타겟 로드 (IntentEvent에서)
			.LoadStore(Target, PropertyId::Param0)

			// 공격 상태 태그 추가 → AnimInstance가 태그를 감지하여 애니메이션 자동 재생
			.AddTag(Self, Tag_Anim_UpperBody_Combat_Attack)
			.AddTag(Self, Tag_Anim_Montage_Attack)
			.WaitAnimEnd(Self)

			// 공격력 로드
			.LoadStore(R0, PropertyId::AttackPower)

			// 피해 적용
			.ApplyDamage(Target, R0)
			.PlayVFXAttached(Target, VFX_HitSpark)
			.PlaySound(Sound_Hit);

		// === CP 회복 (적중 시) ===
		HktSnippetCombat::ResourceGainClamped(B, PropertyId::CP, PropertyId::MaxCP, BasicAttackCPGain);

		// === NextActionFrame 갱신 (공속 기반) ===
		HktSnippetCombat::CooldownUpdateConst(B, BasicAttackRecoveryFrame);

		B	// 공격 상태 태그 제거
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
