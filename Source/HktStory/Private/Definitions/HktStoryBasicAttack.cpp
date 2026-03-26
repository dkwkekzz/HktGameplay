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

	// 공격 애니메이션 (트리거 — 일회성 재생, fire-and-forget)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage trigger tag.");

	// 피격 애니메이션 (트리거 — 일회성 재생, fire-and-forget)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_HitReaction, "Anim.Montage.HitReaction", "Hit reaction montage trigger tag.");

	// 히트 영역 엔티티 (공격자 위치에 스폰, CollisionRadius로 충돌 감지)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_HitArea, "Entity.Area.HitTest", "Hit test area entity spawned at attacker position.");

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

	/** 기본 공격 사거리 (cm) — AttackRange 프로퍼티가 0이면 이 값 사용 */
	static constexpr int32 DefaultAttackRange = 200;

	/**
	 * ================================================================
	 * Basic Attack Flow (충돌 기반 히트 판정)
	 *
	 * 자연어로 읽으면:
	 * "타겟을 바라보고, 공격 모션을 트리거한다 (fire-and-forget).
	 *  공격자 위치에 히트테스트 영역을 절차적으로 생성하고,
	 *  충돌이 감지될 때까지 대기한다.
	 *  닿은 대상에게 데미지를 적용하고, 타격 위치에 이펙트를 생성한다.
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

		B	// === 1. 타겟을 바라본다 ===
			.LookAt(Self, Target)

			// === 2. 공격 애니메이션 트리거 (fire-and-forget) + 스윙 사운드 ===
			.AddTag(Self, Tag_Anim_Montage_Attack)
			.PlaySound(Sound_Swing);

		// === 3. 히트테스트 영역 절차적 생성 ===
		// AttackRange 로드 (0이면 기본값)
		B.LoadStore(R0, PropertyId::AttackRange)
		 .LoadConst(R1, 0)
		 .CmpGt(Flag, R0, R1)
		 .JumpIf(Flag, TEXT("range_ready"))
		 .LoadConst(R0, DefaultAttackRange)
		 .Label(TEXT("range_ready"));
		// R0 = AttackRange

		// 히트 영역 엔티티 스폰 → Spawned 레지스터에 저장
		B.SpawnEntity(Entity_HitArea)
		 .GetPosition(R2, Self)                              // R2 = Self 위치
		 .SetPosition(Spawned, R2)                           // 히트 영역을 공격자 위치에 배치
		 .SaveStoreEntity(Spawned, PropertyId::CollisionRadius, R0);  // CollisionRadius = AttackRange

		// === 4. 충돌 대기 — 물리 시스템이 히트를 감지할 때까지 Yield ===
		B.WaitCollision(Spawned);
		// Hit 레지스터 = 충돌된 엔티티

		// === 5. 히트 영역 제거 (충돌 감지 완료) ===
		B.DestroyEntity(Spawned);

		// === 6. 타격 성공 — 데미지 적용 ===
		B.LoadStore(R0, PropertyId::AttackPower)
		 .ApplyDamage(Hit, R0)
		 .PlaySound(Sound_Hit);

		// === 7. 히트 엔티티 스폰 (타격 위치에 VFX) ===
		B.SpawnEntity(Entity_HitEffect)
		 .GetPosition(R2, Hit)                               // R2 = 피격자 위치
		 .SetPosition(Spawned, R2)                           // 히트 이펙트를 피격 위치에 배치
		 .PlayVFXAttached(Spawned, VFX_HitSpark);

		// === 8. 피격 대상에 히트리액션 (fire-and-forget) ===
		B.AddTag(Hit, Tag_Anim_Montage_HitReaction);

		// === 9. 히트 이펙트 엔티티 제거 ===
		B.DestroyEntity(Spawned);

		// === CP 회복 (적중 시) ===
		HktSnippetCombat::ResourceGainClamped(B, PropertyId::CP, PropertyId::MaxCP, BasicAttackCPGain);

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
