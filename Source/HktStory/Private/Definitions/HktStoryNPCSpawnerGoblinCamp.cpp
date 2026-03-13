// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryNPCSpawnerGoblinCamp
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Spawner_GoblinCamp, "Flow.Spawner.GoblinCamp", "Periodic goblin camp spawner flow.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_NPC_Goblin, "Entity.NPC.Goblin", "Goblin NPC entity.");

	// Tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Entity_NPC, "Entity.NPC", "Generic NPC tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_NPC_Hostile, "NPC.Hostile", "Hostile NPC tag.");

	/**
	 * ================================================================
	 * 고블린 캠프 주기적 스포너 Flow
	 *
	 * 자연어로 읽으면:
	 * "플레이어가 그룹에 있을 때만 고블린을 생성한다.
	 *  인구 상한(5마리)에 도달하면 대기한다.
	 *  10초마다 한 마리씩 스폰한다."
	 *
	 * 서버가 EventTag "Flow.Spawner.GoblinCamp" 을 fire하면 실행.
	 * Param0 = SpawnPosX, Param1 = SpawnPosY
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_Spawner_GoblinCamp)
			.Log(TEXT("GoblinCamp spawner: activated"))

			.Label(TEXT("loop"))
				// Lazy: 플레이어가 그룹에 있을 때만 스폰
				.HasPlayerInGroup(Flag)
				.JumpIfNot(Flag, TEXT("wait"))

				// 인구 체크: 고블린 수 카운트
				.CountByTag(R0, Entity_NPC_Goblin)
				.LoadConst(R1, 5)                           // cap = 5
				.CmpGe(Flag, R0, R1)
				.JumpIf(Flag, TEXT("wait"))

				// NPC 생성 — 스탯을 Flow에서 직접 설정
				.SpawnEntity(Entity_NPC_Goblin)
				.LoadConst(R2, 1)
				.SaveEntityProperty(Spawned, PropertyId::IsNPC, R2)
				.LoadConst(R2, 80)
				.SaveEntityProperty(Spawned, PropertyId::Health, R2)
				.SaveEntityProperty(Spawned, PropertyId::MaxHealth, R2)
				.LoadConst(R2, 15)
				.SaveEntityProperty(Spawned, PropertyId::AttackPower, R2)
				.LoadConst(R2, 3)
				.SaveEntityProperty(Spawned, PropertyId::Defense, R2)
				.LoadConst(R2, 120)
				.SaveEntityProperty(Spawned, PropertyId::MaxSpeed, R2)
				.LoadConst(R2, 0)
				.SaveEntityProperty(Spawned, PropertyId::Team, R2)

				// 태그 부여
				.AddTag(Spawned, Tag_Entity_NPC)
				.AddTag(Spawned, Entity_NPC_Goblin)
				.AddTag(Spawned, Tag_NPC_Hostile)

				// 위치 설정 (이벤트 파라미터에서 읽기)
				.LoadStore(R3, PropertyId::Param0)          // SpawnPosX
				.LoadStore(R4, PropertyId::Param1)          // SpawnPosY
				.LoadConst(R5, 0)                           // Z = ground
				.SetPosition(Spawned, R3)

				.Log(TEXT("GoblinCamp: goblin spawned"))

			.Label(TEXT("wait"))
				.WaitSeconds(10.0f)
				.Jump(TEXT("loop"))
			.BuildAndRegister();
	}
}
