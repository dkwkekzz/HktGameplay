// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktFlowBuilder.h"
#include "HktPropertyIds.h"
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
			.SpawnEntity(Entity_Character_Player)
			.Move(Self, Spawned)                        // Self = 새로 생성된 캐릭터

			// 위치 설정 (이벤트의 Location에서)
			.LoadStore(R0, PropertyId::TargetPosX)
			.LoadStore(R1, PropertyId::TargetPosY)
			.LoadStore(R2, PropertyId::TargetPosZ)
			.SetPosition(Self, R0)

			// 플레이어 속성 초기화 (기본값 또는 저장된 값)
			// OwnerPlayerHash 설정 (플레이어 UID를 해시로 변환)
			// Param0와 Param1에서 플레이어 UID를 복원하여 OwnerPlayerHash에 설정
			.LoadStore(R3, PropertyId::Param0)         // 플레이어 UID 하위 32비트
			.LoadStore(R4, PropertyId::Param1)         // 플레이어 UID 상위 32비트
			// R3와 R4를 조합하여 해시 생성 (간단한 해시 함수)
			.Mul(R5, R4, R3)                           // R5 = R4 * R3 (간단한 해시)
			.SaveEntityProperty(Self, PropertyId::OwnerPlayerHash, R5)

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

			.Log(TEXT("PlayerInWorld: 준비 완료, 상태 유지"))

			.BuildAndRegister();
	}
}