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

	// 공격 애니메이션 (트리거 — 일회성 재생)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage trigger tag.");

	// 피격 애니메이션 (트리거 — 일회성 재생)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_HitReaction, "Anim.Montage.HitReaction", "Hit reaction montage trigger tag.");

	// 히트 엔티티 (타격 위치에 스폰되어 VFX를 재생하는 임시 엔티티)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_HitEffect, "Entity.Effect.Hit", "Hit effect entity spawned at impact point.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_HitSpark, "VFX.Niagara.HitSpark", "Melee hit spark VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Swing, "Sound.Swing", "Melee swing sound.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Hit, "Sound.Hit", "Melee hit sound.");

	/** CP 기본 공격 적중 시 회복량 */
	static constexpr int32 BasicAttackCPGain = 10;

	/** 기본 공격 후딜레이 (프레임) — AttackSpeed로 나뉨 */
	static constexpr int32 BasicAttackRecoveryFrame = 30;

	/** 공격 애니메이션 총 길이 (초) */
	static constexpr float AttackAnimDuration = 0.8f;

	/** 타격 판정 타이밍 (초) — 애니메이션 시작 후 히트 판정 시점 */
	static constexpr float HitTimingSec = 0.3f;

	/** 히트 엔티티 잔존 시간 (초) — VFX 재생 후 자동 제거 */
	static constexpr float HitEffectLifetime = 0.5f;

	/** 기본 공격 사거리 (cm) — AttackRange 프로퍼티가 0이면 이 값 사용 */
	static constexpr int32 DefaultAttackRange = 200;

	/**
	 * ================================================================
	 * Basic Attack Flow (재구현)
	 *
	 * 자연어로 읽으면:
	 * "타겟을 바라보고, 공격 모션을 트리거한다.
	 *  타격 타이밍에 사거리 내 적을 검색하여 데미지를 적용하고,
	 *  타격 위치에 히트 엔티티를 스폰하여 이펙트를 재생한다.
	 *  맞은 대상은 피격 모션을 재생한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_BasicAttack);
		B.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool
			{
				if (!WS.IsValidEntity(E.SourceEntity))
					return false;
				int32 NextFrame = WS.GetProperty(E.SourceEntity, PropertyId::NextActionFrame);
				return WS.FrameNumber >= NextFrame;
			})
			.Log(TEXT("BasicAttack: 공격 시작"));

		// === 공속 기반 쿨타임 검증 (서버 권위적 이중 검증) ===
		HktSnippetCombat::CooldownCheck(B, TEXT("fail"));

		B	// Target 레지스터는 VM 초기화 시 Event.TargetEntity로 이미 설정됨.
			// (DispatchEvent는 SourceEntity/TargetEntity를 그대로 전달)

			// === 1. 타겟을 바라본다 ===
			.LookAt(Self, Target)

			// === 2. 공격 애니메이션 트리거 + 스윙 사운드 ===
			.AddTag(Self, Tag_Anim_Montage_Attack)
			.PlaySound(Sound_Swing)

			// === 3. 타격 타이밍까지 대기 ===
			.WaitSeconds(HitTimingSec);

		// === 4. 히트테스트 — 사거리 내 적 탐색 ===
		// AttackRange 로드 (0이면 기본값)
		B.LoadStore(R0, PropertyId::AttackRange)
		 .LoadConst(R1, 0)
		 .CmpGt(Flag, R0, R1)
		 .JumpIf(Flag, TEXT("range_ready"))
		 .LoadConst(R0, DefaultAttackRange)
		 .Label(TEXT("range_ready"));
		// R0 = AttackRange (FindInRadius의 상수 파라미터로 사용 불가 → 직접 거리 비교)

		// 히트 판정: 타겟과의 거리 확인
		B.GetDistance(R1, Self, Target)  // R1 = 거리
		 .CmpLe(Flag, R1, R0)           // 사거리 내?
		 .JumpIfNot(Flag, TEXT("miss"));

		// === 5. 타격 성공 — 데미지 적용 ===
		B.LoadStore(R0, PropertyId::AttackPower)
		 .ApplyDamage(Target, R0)
		 .PlaySound(Sound_Hit);

		// === 6. 히트 엔티티 스폰 (타격 위치에 VFX) ===
		B.SpawnEntity(Entity_HitEffect)
		 .GetPosition(R2, Target)                  // R2,R3,R4 = 타겟 위치
		 .SetPosition(Spawned, R2)                 // 히트 엔티티를 타겟 위치에 배치
		 .PlayVFXAttached(Spawned, VFX_HitSpark);

		// === 7. 피격 대상에 히트리액션 애니메이션 트리거 ===
		HktSnippetCombat::AnimTrigger(B, Target, Tag_Anim_Montage_HitReaction, HitEffectLifetime);

		// === 8. 히트 엔티티 제거 ===
		B.DestroyEntity(Spawned);

		// === CP 회복 (적중 시) ===
		HktSnippetCombat::ResourceGainClamped(B, PropertyId::CP, PropertyId::MaxCP, BasicAttackCPGain);

		B.Jump(TEXT("post_attack"));

		// === 빗나감 ===
		B.Label(TEXT("miss"))
		 .Log(TEXT("BasicAttack: 사거리 밖 — 빗나감"));

		// === 후처리 (적중/빗나감 공통) ===
		B.Label(TEXT("post_attack"));

		// 남은 애니메이션 대기 후 태그 제거
		B.WaitSeconds(AttackAnimDuration - HitTimingSec)
		 .RemoveTag(Self, Tag_Anim_Montage_Attack);

		// === NextActionFrame 갱신 (공속 기반) ===
		HktSnippetCombat::CooldownUpdateConst(B, BasicAttackRecoveryFrame);

		B.Log(TEXT("BasicAttack: 완료"))
		 .Halt()

		.Label(TEXT("fail"))
			.Log(TEXT("BasicAttack: 쿨타임 중 — 실패"))
			.Fail()
		.BuildAndRegister();
	}
}
