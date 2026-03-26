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

	// Hit Story (피격 처리 — 데미지 + VFX + 리액션)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Hit, "Story.Event.Hit.Basic", "Basic hit story — damage + VFX + reaction.");

	// 공격 애니메이션 (트리거 — fire-and-forget)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Attack, "Anim.Montage.Attack", "Basic attack montage trigger tag.");

	// 히트 영역 엔티티 (공격자 위치에 스폰, CollisionRadius로 충돌 감지)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_HitArea, "Entity.Area.HitTest", "Hit test area entity spawned at attacker position.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Swing, "Sound.Swing", "Melee swing sound.");

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
	 *  닿은 대상에게 HitStory를 발행하여 데미지와 이펙트를 위임한다."
	 *
	 * 쿨타임/사거리 검증은 AttackEngage에서 이미 완료됨.
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_BasicAttack);
		B.Log(TEXT("BasicAttack: 공격 시작"))

			// === 1. 타겟을 바라본다 ===
			.LookAt(Self, Target)

			// === 2. 공격 애니메이션 트리거 (fire-and-forget) + 스윙 사운드 ===
			.AddTag(Self, Tag_Anim_Montage_Attack)
			.PlaySound(Sound_Swing)

			// === 3. 히트테스트 영역 절차적 생성 ===
			// AttackRange 로드 (0이면 기본값)
			.LoadStore(R0, PropertyId::AttackRange)
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
		// Self-collision 필터: HitArea가 공격자 자신과 충돌할 수 있으므로 제외
	B.Label(TEXT("wait_hit"))
		 .WaitCollision(Spawned)
		 .Move(R3, Hit)
		 .Move(R4, Self)
		 .CmpEq(Flag, R3, R4)
		 .JumpIf(Flag, TEXT("wait_hit"));
		// Hit 레지스터 = 충돌된 엔티티 (Self 제외)

		// === 5. 히트 영역 제거 ===
		B.DestroyEntity(Spawned);

		// === 6. 피격 대상에게 HitStory 발행 ===
		B.DispatchEventTo(Story_Hit, Hit);

		// === 7. NextActionFrame 갱신 (공속 기반) ===
		HktSnippetCombat::CooldownUpdateConst(B, BasicAttackRecoveryFrame);

		B.Log(TEXT("BasicAttack: 완료"))
		 .Halt()
		.BuildAndRegister();
	}
}
