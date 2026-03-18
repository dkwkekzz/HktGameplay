// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryPlayerInWorld
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_PlayerInWorld, "State.Player.InWorld", "Player in world state flow.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Character_Player, "Entity.Character.Player", "Player character entity.");

	// State Tags — AnimInstance가 태그를 보고 애니메이션을 자동 재생
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Action_Spawn, "Anim.FullBody.Action.Spawn", "Spawn intro state tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Locomotion_Idle, "Anim.FullBody.Locomotion.Idle", "Idle state tag.");

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
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_PlayerInWorld)
			.Log(TEXT("PlayerInWorld: 플레이어 캐릭터 생성"))

			// 캐릭터 엔티티 생성
			.SpawnEntity(Entity_Character_Player)
			.Move(Self, Spawned)                        // Self = 새로 생성된 캐릭터

			// 위치 설정 (이벤트의 Location에서)
			.LoadConst(R0, 0.f)
			.LoadConst(R1, 0.f)
			.LoadConst(R2, 0.f)
			.SetPosition(Self, R0)

			// 스폰 이펙트
			.PlayVFXAttached(Self, VFX_SpawnEffect)
			.PlaySound(Sound_Spawn)

			// 플레이어 Stance 설정
			.SetStance(Self, HktStance::Spear)                          

			// === 초기 아이템: 목검 ===
			.Log(TEXT("PlayerInWorld: 목검 지급"))
			.SpawnEntity(Entity_Item_WoodenSword)
			.SaveStoreEntity(Spawned, PropertyId::OwnerEntity, Self)     // 소유자 = 플레이어
			.LoadConst(Temp, 1).SaveStoreEntity(Spawned, PropertyId::ItemState, Temp)       // InBag
			.LoadConst(Temp, 100).SaveStoreEntity(Spawned, PropertyId::ItemId, Temp)        // 목검 ID
			.LoadConst(Temp, 0).SaveStoreEntity(Spawned, PropertyId::BagSlot, Temp)         // 가방 슬롯 0
			.LoadConst(Temp, -1).SaveStoreEntity(Spawned, PropertyId::ActionSlot, Temp)     // 미등록
			.LoadConst(Temp, 5).SaveStoreEntity(Spawned, PropertyId::AttackPower, Temp)     // 공격력 5
			.AddTag(Spawned, Tag_Weapon_Sword)

			.Log(TEXT("PlayerInWorld: 준비 완료, 상태 유지"))

			.BuildAndRegister();
	}
}
