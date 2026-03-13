// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryNPCSpawnerWave
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Spawner_Wave_Arena, "Flow.Spawner.Wave.Arena", "Wave-based arena spawner flow.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_NPC_Goblin, "Entity.NPC.Goblin", "Goblin NPC entity.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_NPC_Skeleton, "Entity.NPC.Skeleton", "Skeleton NPC entity.");

	// Tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Entity_NPC, "Entity.NPC", "Generic NPC tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_NPC_Hostile, "NPC.Hostile", "Hostile NPC tag.");

	/**
	 * ================================================================
	 * 웨이브 스포너 Flow
	 *
	 * 자연어로 읽으면:
	 * "Wave 1: 고블린 3마리 스폰 → 전멸 대기 →
	 *  Wave 2: 스켈레톤 2마리 스폰 → 전멸 대기 → 완료."
	 *
	 * 서버가 EventTag "Flow.Spawner.Wave.Arena" 을 fire.
	 * Self = 아레나 중심 앵커 엔티티
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_Spawner_Wave_Arena)
			.Log(TEXT("Wave spawner: starting"))
			.LoadConst(R0, 0)                               // R0 = zero constant

			// === Wave 1: 고블린 3마리 ===
			.Label(TEXT("wave1"))
				.Log(TEXT("Wave 1: spawning goblins"))
				.LoadConst(R1, 3)                           // R1 = spawn count
				.LoadConst(R2, 0)                           // R2 = counter

			.Label(TEXT("wave1_loop"))
				.CmpGe(Flag, R2, R1)
				.JumpIf(Flag, TEXT("wave1_wait"))

				.SpawnEntity( Entity_NPC_Goblin)
				.LoadConst(R3, 1)
				.SaveEntityProperty(Spawned, PropertyId::IsNPC, R3)
				.LoadConst(R3, 80)
				.SaveEntityProperty(Spawned, PropertyId::Health, R3)
				.SaveEntityProperty(Spawned, PropertyId::MaxHealth, R3)
				.LoadConst(R3, 15)
				.SaveEntityProperty(Spawned, PropertyId::AttackPower, R3)
				.LoadConst(R3, 0)
				.SaveEntityProperty(Spawned, PropertyId::Team, R3)
				.AddTag(Spawned, Tag_Entity_NPC)
				.AddTag(Spawned, Entity_NPC_Goblin)
				.AddTag(Spawned, Tag_NPC_Hostile)
				.GetPosition(R3, Self)
				.SetPosition(Spawned, R3)

				.AddImm(R2, R2, 1)
				.Jump(TEXT("wave1_loop"))

			// Wave 1 전멸 대기
			.Label(TEXT("wave1_wait"))
				.CountByTag(R3, Entity_NPC_Goblin)
				.CmpEq(Flag, R3, R0)                       // count == 0?
				.JumpIf(Flag, TEXT("wave2"))
				.WaitSeconds(2.0f)
				.Jump(TEXT("wave1_wait"))

			// === Wave 2: 스켈레톤 2마리 ===
			.Label(TEXT("wave2"))
				.Log(TEXT("Wave 2: spawning skeletons"))
				.LoadConst(R1, 2)
				.LoadConst(R2, 0)

			.Label(TEXT("wave2_loop"))
				.CmpGe(Flag, R2, R1)
				.JumpIf(Flag, TEXT("wave2_wait"))

				.SpawnEntity( Entity_NPC_Skeleton)
				.LoadConst(R3, 1)
				.SaveEntityProperty(Spawned, PropertyId::IsNPC, R3)
				.LoadConst(R3, 60)
				.SaveEntityProperty(Spawned, PropertyId::Health, R3)
				.SaveEntityProperty(Spawned, PropertyId::MaxHealth, R3)
				.LoadConst(R3, 20)
				.SaveEntityProperty(Spawned, PropertyId::AttackPower, R3)
				.LoadConst(R3, 0)
				.SaveEntityProperty(Spawned, PropertyId::Team, R3)
				.AddTag(Spawned, Tag_Entity_NPC)
				.AddTag(Spawned, Entity_NPC_Skeleton)
				.AddTag(Spawned, Tag_NPC_Hostile)
				.GetPosition(R3, Self)
				.SetPosition(Spawned, R3)

				.AddImm(R2, R2, 1)
				.Jump(TEXT("wave2_loop"))

			// Wave 2 전멸 대기
			.Label(TEXT("wave2_wait"))
				.CountByTag(R4, Entity_NPC_Skeleton)
				.CmpEq(Flag, R4, R0)
				.JumpIf(Flag, TEXT("complete"))
				.WaitSeconds(2.0f)
				.Jump(TEXT("wave2_wait"))

			.Label(TEXT("complete"))
				.Log(TEXT("All waves complete"))
				.Halt()
			.BuildAndRegister();
	}
}
