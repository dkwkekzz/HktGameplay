// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStoryCharacterSpawn
{
	// Story Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_CharacterSpawn, "Event.Character.Spawn", "Character spawn event flow.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Character_Player, "Entity.Character.Player", "Player character entity.");

	// State Tags — AnimInstance가 태그를 보고 애니메이션을 자동 재생
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Action_Spawn, "Anim.FullBody.Action.Spawn", "Spawn intro state tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_Montage_Intro, "Anim.Montage.Intro", "Character intro montage state tag.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_Anim_FullBody_Locomotion_Idle, "Anim.FullBody.Locomotion.Idle", "Idle state tag.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_SpawnEffect, "VFX.SpawnEffect", "Character spawn VFX.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_EquipGlow, "VFX.EquipGlow", "Item equip glow VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Spawn, "Sound.Spawn", "Character spawn sound.");

	// Item (장비 아이템)
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_Sword, "Entity.Item.Sword", "Sword weapon item entity.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Item_Shield, "Entity.Item.Shield", "Shield item entity.");

	/**
	 * ================================================================
	 * 캐릭터 입장 Flow
	 *
	 * 자연어로 읽으면:
	 * "캐릭터를 생성하고 스폰 상태 태그를 추가한다.
	 *  0.5초 후 장비를 생성하고 인트로 몽타주 태그를 추가한다.
	 *  완료 후 Idle 상태 태그로 전환한다."
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		Story(Story_CharacterSpawn)
			.Log(TEXT("CharacterSpawn: 캐릭터 생성"))

			// 캐릭터 스폰
			.SpawnEntity(Entity_Character_Player)
			.Move(Self, Spawned)                        // Self = 새로 생성된 캐릭터

			// 스폰 위치 설정 (IntentEvent에서)
			.LoadStore(R0, PropertyId::TargetPosX)
			.LoadStore(R1, PropertyId::TargetPosY)
			.LoadStore(R2, PropertyId::TargetPosZ)
			.SetPosition(Self, R0)

			// 스폰 이펙트
			.PlayVFXAttached(Self, VFX_SpawnEffect)
			.PlaySound(Sound_Spawn)

			// 스폰 상태 태그 추가 → AnimInstance가 태그를 감지하여 스폰 애니메이션 자동 재생
			.AddTag(Self, Tag_Anim_FullBody_Action_Spawn)

			// 0.5초 대기
			.WaitSeconds(0.5f)

			// === 장비 아이템 생성 (Entity로 통합) ===
			.Log(TEXT("CharacterSpawn: 장비 아이템 생성"))

			// 메인 무기 (슬롯 0)
			.SpawnEntity(Entity_Item_Sword)
			.LoadConst(R3, 0)
			.SaveEntityProperty(Spawned, PropertyId::ActionSlot, R3)
			.PlayVFXAttached(Spawned, VFX_EquipGlow)

			// 보조 장비 (슬롯 1)
			.SpawnEntity(Entity_Item_Shield)
			.LoadConst(R3, 1)
			.SaveEntityProperty(Spawned, PropertyId::ActionSlot, R3)

			// 인트로 몽타주 상태 태그 추가
			.AddTag(Self, Tag_Anim_Montage_Intro)
			.WaitAnimEnd(Self)

			// 준비 완료 - 스폰/인트로 태그 제거, Idle 상태 태그로 전환
			.RemoveTag(Self, Tag_Anim_FullBody_Action_Spawn)
			.RemoveTag(Self, Tag_Anim_Montage_Intro)
			.AddTag(Self, Tag_Anim_FullBody_Locomotion_Idle)

			.Log(TEXT("CharacterSpawn: 준비 완료"))
			.Halt()
			.BuildAndRegister();
	}
}
