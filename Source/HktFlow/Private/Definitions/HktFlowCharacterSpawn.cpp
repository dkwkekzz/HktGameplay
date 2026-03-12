// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktCoreProperties.h"
#include "HktFlowRegistry.h"
#include "NativeGameplayTags.h"

namespace HktFlowCharacterSpawn
{
	// Flow Name
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Flow_CharacterSpawn, "Event.Character.Spawn", "Character spawn event flow.");

	// Entity
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Entity_Character_Player, "Entity.Character.Player", "Player character entity.");

	// Anim — 태그 계층에 레이어 포함
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_FullBody_Action_Spawn, "Anim.FullBody.Action.Spawn", "Spawn intro animation.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Montage_Intro, "Anim.Montage.Intro", "Character intro montage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_FullBody_Locomotion_Idle, "Anim.FullBody.Locomotion.Idle", "Idle animation.");

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_SpawnEffect, "VFX.SpawnEffect", "Character spawn VFX.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_EquipGlow, "VFX.EquipGlow", "Equipment glow VFX.");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_Spawn, "Sound.Spawn", "Character spawn sound.");

	// Equipment
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Weapon_Sword, "Equipment.Weapon.Sword", "Sword weapon equipment.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Shield, "Equipment.Shield", "Shield equipment.");

	/**
	 * ================================================================
	 * 캐릭터 입장 Flow
	 *
	 * 자연어로 읽으면:
	 * "캐릭터를 생성하고 스폰 애니메이션을 재생한다.
	 *  0.5초 후 장비를 생성하고 인트로 애니메이션을 재생한다."
	 * ================================================================
	 */
	HKT_REGISTER_FLOW_BODY()
	{
		using namespace Reg;

		Flow(Flow_CharacterSpawn)
			.Log(TEXT("CharacterSpawn: 캐릭터 생성"))

			// 캐릭터 스폰
			.SpawnEntity(HktType::Unit, Entity_Character_Player)
			.Move(Self, Spawned)                        // Self = 새로 생성된 캐릭터

			// 스폰 위치 설정 (IntentEvent에서)
			.LoadStore(R0, PropertyId::TargetPosX)
			.LoadStore(R1, PropertyId::TargetPosY)
			.LoadStore(R2, PropertyId::TargetPosZ)
			.SetPosition(Self, R0)

			// 스폰 이펙트
			.PlayVFXAttached(Self, VFX_SpawnEffect)
			.PlaySound(Sound_Spawn)

			// 스폰 애니메이션
			.PlayAnim(Self, Anim_FullBody_Action_Spawn)

			// 0.5초 대기
			.WaitSeconds(0.5f)

			// === 장비 생성 ===
			.Log(TEXT("CharacterSpawn: 장비 생성"))

			// 메인 무기 (슬롯 0)
			.SpawnEquipment(Self, 0, Equipment_Weapon_Sword)
			.PlayVFXAttached(Spawned, VFX_EquipGlow)

			// 보조 장비 (슬롯 1)
			.SpawnEquipment(Self, 1, Equipment_Shield)

			// 인트로 애니메이션
			.PlayAnimMontage(Self, Anim_Montage_Intro)
			.WaitAnimEnd(Self)

			// 준비 완료 - Idle 상태로 전환
			.PlayAnim(Self, Anim_FullBody_Locomotion_Idle)

			.Log(TEXT("CharacterSpawn: 준비 완료"))
			.Halt()
			.BuildAndRegister();
	}
}
