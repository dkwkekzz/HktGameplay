// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryNPCSpawnerProximity
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_Spawner_DungeonEntrance, "Flow.Spawner.DungeonEntrance", "Proximity-triggered dungeon entrance spawner.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_NPC_Skeleton, "Entity.NPC.Skeleton", "Skeleton NPC entity.");

	// Tags
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Entity_NPC, "Entity.NPC", "Generic NPC tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_NPC_Hostile, "NPC.Hostile", "Hostile NPC tag.");

	/**
	 * ================================================================
	 * 근접 트리거 스포너 Flow (던전 입구)
	 *
	 * 자연어로 읽으면:
	 * "Self(앵커 엔티티) 반경 20m 안에 적이 있을 때
	 *  스켈레톤 인구가 3마리 미만이면 스켈레톤을 생성한다.
	 *  5초마다 체크한다."
	 *
	 * 서버가 EventTag "Flow.Spawner.DungeonEntrance" 을 fire.
	 * Self = zone 앵커 엔티티 (위치만 있는 Unit)
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_Spawner_DungeonEntrance)
			.Log(TEXT("DungeonEntrance proximity spawner: monitoring"))

			.Label(TEXT("check"))
				// 플레이어 존재 확인
				.HasPlayerInGroup(Flag)
				.JumpIfNot(Flag, TEXT("sleep"))

				// 반경 내 엔티티 검색 (20m = 2000cm)
				.FindInRadius(Self, 2000)
				.NextFound()
				.JumpIfNot(Flag, TEXT("sleep"))

				// 인구 체크
				.CountByTag(R0, Entity_NPC_Skeleton)
				.LoadConst(R1, 3)
				.CmpGe(Flag, R0, R1)
				.JumpIf(Flag, TEXT("sleep"))

				// 스켈레톤 NPC 직접 생성
				.SpawnEntity(Entity_NPC_Skeleton)
				.LoadConst(R2, 1)
				.SaveEntityProperty(Spawned, PropertyId::IsNPC, R2)
				.LoadConst(R2, 60)
				.SaveEntityProperty(Spawned, PropertyId::Health, R2)
				.SaveEntityProperty(Spawned, PropertyId::MaxHealth, R2)
				.LoadConst(R2, 20)
				.SaveEntityProperty(Spawned, PropertyId::AttackPower, R2)
				.LoadConst(R2, 2)
				.SaveEntityProperty(Spawned, PropertyId::Defense, R2)
				.LoadConst(R2, 100)
				.SaveEntityProperty(Spawned, PropertyId::MaxSpeed, R2)
				.LoadConst(R2, 0)
				.SaveEntityProperty(Spawned, PropertyId::Team, R2)

				// 태그 부여
				.AddTag(Spawned, Tag_Entity_NPC)
				.AddTag(Spawned, Entity_NPC_Skeleton)
				.AddTag(Spawned, Tag_NPC_Hostile)

				// 위치: 앵커 위치에 스폰
				.GetPosition(R3, Self)
				.SetPosition(Spawned, R3)

				.Log(TEXT("DungeonEntrance: skeleton spawned"))

			.Label(TEXT("sleep"))
				.WaitSeconds(5.0f)
				.Jump(TEXT("check"))
			.BuildAndRegister();
	}
}
