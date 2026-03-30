// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryEventParams.h"
#include "HktStoryRegistry.h"
#include "HktStoryTags.h"
#include "HktRuntimeTags.h"
#include "NativeGameplayTags.h"
#include "Snippets/HktSnippetNPC.h"

namespace HktStoryNPCSpawnerWave
{
	using namespace HktStoryTags;
	using namespace HktGameplayTags;

	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Spawner_Wave_Arena, "Story.Flow.Spawner.Wave.Arena", "Wave-based arena spawner flow.");

	/**
	 * ================================================================
	 * 웨이브 스포너 Flow
	 *
	 * 자연어로 읽으면:
	 * "Wave 1: 고블린 3마리 스폰 → 전멸 대기 →
	 *  Wave 2: 스켈레톤 2마리 스폰 → 전멸 대기 → 완료."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_Spawner_Wave_Arena);

		FHktScopedReg r0(B);            // zero constant
		FHktScopedReg r1(B);            // spawn count
		FHktScopedReg r2(B);            // counter
		FHktScopedReg r3(B);            // count by tag (wave1)
		FHktScopedReg r4(B);            // count by tag (wave2)
		FHktScopedRegBlock pos(B, 3);   // 스폰 위치 (X, Y, Z) — 연속 보장

		B.Log(TEXT("Wave spawner: starting"))
			.LoadConst(r0, 0)                               // r0 = zero constant
			// 이벤트 파라미터에서 스폰 위치 로드
			.LoadStore(pos, SpawnerParams::SpawnPosX)
			.LoadStore(pos + 1, SpawnerParams::SpawnPosY)
			.LoadConst(pos + 2, 0)                          // Z = ground

			// === Wave 1: 고블린 3마리 ===
			.Label(TEXT("wave1"))
				.Log(TEXT("Wave 1: spawning goblins"))
				.LoadConst(r1, 3)                           // r1 = spawn count
				.LoadConst(r2, 0)                           // r2 = counter

			.Label(TEXT("wave1_loop"))
				.CmpGe(Flag, r2, r1)
				.JumpIf(Flag, TEXT("wave1_wait"))

				;

		// 고블린 생성 + 스탯 설정 + 위치 지정
		HktSnippetNPC::SpawnNPCAtPosition(B, Entity_NPC_Goblin, { 80, 15, 0, 0, 0 }, pos);

		B

				.AddImm(r2, r2, 1)
				.Jump(TEXT("wave1_loop"))

			// Wave 1 전멸 대기
			.Label(TEXT("wave1_wait"))
				.CountByTag(r3, Entity_NPC_Goblin)
				.CmpEq(Flag, r3, r0)                       // count == 0?
				.JumpIf(Flag, TEXT("wave2"))
				.WaitSeconds(2.0f)
				.Jump(TEXT("wave1_wait"))

			// === Wave 2: 스켈레톤 2마리 ===
			.Label(TEXT("wave2"))
				.Log(TEXT("Wave 2: spawning skeletons"))
				.LoadConst(r1, 2)
				.LoadConst(r2, 0)

			.Label(TEXT("wave2_loop"))
				.CmpGe(Flag, r2, r1)
				.JumpIf(Flag, TEXT("wave2_wait"))

				;

		// 스켈레톤 생성 + 스탯 설정 + 위치 지정
		HktSnippetNPC::SpawnNPCAtPosition(B, Entity_NPC_Skeleton, { 60, 20, 0, 0, 0 }, pos);

		B

				.AddImm(r2, r2, 1)
				.Jump(TEXT("wave2_loop"))

			// Wave 2 전멸 대기
			.Label(TEXT("wave2_wait"))
				.CountByTag(r4, Entity_NPC_Skeleton)
				.CmpEq(Flag, r4, r0)
				.JumpIf(Flag, TEXT("complete"))
				.WaitSeconds(2.0f)
				.Jump(TEXT("wave2_wait"))

			.Label(TEXT("complete"))
				.Log(TEXT("All waves complete"))
				.Halt()
			.BuildAndRegister();
	}
}
