// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktCoreProperties.h"
#include "HktFlowRegistry.h"
#include "NativeGameplayTags.h"

namespace HktFlowPlayerInWorld
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_PlayerInWorld, "State.Player.InWorld", "Player in world state flow.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Character_Player, "Entity.Character.Player", "Player character entity.");

	// Anim
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Spawn, "Anim.Spawn", "Spawn intro animation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Idle, "Anim.Idle", "Idle animation.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_SpawnEffect, "VFX.SpawnEffect", "Character spawn VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Spawn, "Sound.Spawn", "Character spawn sound.");

	// Item
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_WoodenSword, "Entity.Item.WoodenSword", "Wooden sword starter item.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Weapon_Sword, "Tag.Weapon.Sword", "Sword weapon tag.");

	/**
	 * ================================================================
	 * 플레이어 월드 진입 상태 Flow
	 *
	 * 자연어로 읽으면:
	 * "플레이어 캐릭터를 생성하고 위치를 설정한다.
	 *  플레이어 속성을 초기화한다.
	 *  캐릭터가 파괴되기 전까지 이 상태를 유지한다."
	 *
	 * 이 플로우는 무한 루프로 실행되어 VM이 완료되지 않도록 하며,
	 * WorldState.ActiveEvents에 이벤트가 유지됩니다.
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;

		Flow(Flow_PlayerInWorld)
			.Log(TEXT("PlayerInWorld: 플레이어 캐릭터 생성"))

			// 캐릭터 엔티티 생성
			.SpawnEntity(HktType::Unit, Entity_Character_Player)
			.Move(Self, Spawned)                        // Self = 새로 생성된 캐릭터

			// 위치 설정 (이벤트의 Location에서)
			.LoadConst(R0, 0.f)
			.LoadConst(R1, 0.f)
			.LoadConst(R2, 0.f)
			.SetPosition(Self, R0)

			// OwnedPlayerUid는 SpawnEntity에서 Runtime.PlayerUid로 자동 설정됨

			// 기본 체력 설정 (필요시)
			// .LoadConst(R4, 100)
			// .SaveEntityProperty(Self, PropertyId::Health, R4)
			// .SaveEntityProperty(Self, PropertyId::MaxHealth, R4)

			// 스폰 이펙트
			.PlayVFXAttached(Self, VFX_SpawnEffect)
			.PlaySound(Sound_Spawn)

			// 스폰 애니메이션
			.PlayAnim(Self, Anim_Spawn)
			.WaitSeconds(0.5f)

			// Idle 상태로 전환
			.PlayAnim(Self, Anim_Idle)

			// === 초기 아이템: 목검 ===
			.Log(TEXT("PlayerInWorld: 목검 지급"))
			.SpawnEntity(HktType::Equipment, Entity_Item_WoodenSword)
			.SaveEntityProperty(Spawned, PropertyId::OwnerEntity, Self)  // 소유자 = 플레이어
			.LoadConst(R3, 1)
			.SaveEntityProperty(Spawned, PropertyId::ItemState, R3)      // InBag
			.LoadConst(R3, 100)
			.SaveEntityProperty(Spawned, PropertyId::ItemId, R3)         // 목검 ID
			.LoadConst(R3, 0)
			.SaveEntityProperty(Spawned, PropertyId::BagSlot, R3)        // 가방 슬롯 0
			.LoadConst(R3, -1)
			.SaveEntityProperty(Spawned, PropertyId::ActionSlot, R3)     // 미등록
			.LoadConst(R3, 5)
			.SaveEntityProperty(Spawned, PropertyId::AttackPower, R3)    // 공격력 5
			.AddTag(Spawned, Tag_Weapon_Sword)

			.Log(TEXT("PlayerInWorld: 준비 완료, 상태 유지"))

			.BuildAndRegister();
	}
}