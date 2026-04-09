// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreDefs.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "HktStoryTags.h"
#include "HktStoryEventParams.h"
#include "NativeGameplayTags.h"

namespace HktStoryVoxelShatter
{
	using namespace HktStoryTags;

	// VFX
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(VFX_VoxelShatter, "VFX.Niagara.VoxelShatter", "Glass voxel shatter VFX (multiple fragments).");

	// Sound
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Sound_VoxelShatter, "Sound.VoxelShatter", "Glass voxel shatter sound.");

	/** 파편 수 */
	static constexpr int32 FragmentCount = 3;

	/** 파편별 상향 속도 (cm/s) — 흩뿌리기 효과 */
	static constexpr int32 JumpVelZ_Fragment1 = 300;
	static constexpr int32 JumpVelZ_Fragment2 = 200;
	static constexpr int32 JumpVelZ_Fragment3 = 250;

	/**
	 * ================================================================
	 * 유리 복셀 파쇄 Flow (Glass)
	 *
	 * 자연어로 읽으면:
	 * "유리 파편 VFX/사운드를 재생한 뒤,
	 *  3개의 유리 파편 엔티티를 생성한다.
	 *  각 파편은 공중에서 튕기며 흩어진다."
	 *
	 * InteractTerrain에서 유리 복셀 제거 후 자동 dispatch됨.
	 * Self = 공격 주체, TargetPos = 복셀 중심, Param0 = TypeId
	 * ================================================================
	 */
	HKT_REGISTER_STORY_BODY()
	{
		using namespace Reg;

		auto B = Story(Story_Voxel_Shatter);

		// Precondition: 현재는 무조건 통과 (확장점 — 향후 "충격 강도 >= N" 등 조건 추가 가능)
		B.BeginPrecondition()
			.LoadConst(Flag, 1)
		.EndPrecondition();

		FHktScopedRegBlock pos(B, 3);   // 복셀 위치 (X, Y, Z)
		FHktScopedReg typeId(B);        // 원래 TypeId

		B	// 복셀 위치 및 TypeId 읽기
			.LoadStore(pos,                                     PropertyId::TargetPosX)
			.LoadStore(static_cast<RegisterIndex>(pos + 1),     PropertyId::TargetPosY)
			.LoadStore(static_cast<RegisterIndex>(pos + 2),     PropertyId::TargetPosZ)
			.LoadStore(typeId,                                  VoxelBreakParams::TypeId)

			// 파쇄 VFX/사운드
			.PlayVFX(pos, VFX_VoxelShatter)
			.PlaySoundAtLocation(pos, Sound_VoxelShatter)

			// === 파편 1 (지형 복원 책임) ===
			.SpawnEntity(Entity_Debris_Glass)
			.SetPosition(Spawned, pos)
			.SaveStoreEntity(Spawned, PropertyId::TerrainTypeId, typeId)
			.SaveStoreEntity(Spawned, PropertyId::DebrisOriginX, pos)
			.SaveStoreEntity(Spawned, PropertyId::DebrisOriginY, static_cast<RegisterIndex>(pos + 1))
			.SaveStoreEntity(Spawned, PropertyId::DebrisOriginZ, static_cast<RegisterIndex>(pos + 2))
			.SaveConstEntity(Spawned, PropertyId::Health, 1)
			.SaveConstEntity(Spawned, PropertyId::MaxHealth, 1)
			.SaveConstEntity(Spawned, PropertyId::IsGrounded, 0)
			.SaveConstEntity(Spawned, PropertyId::JumpVelZ, JumpVelZ_Fragment1)
			.DispatchEventFrom(Story_Debris_Lifecycle, Spawned)

			// === 파편 2 (시각 전용 — DebrisOrigin 미설정, 지형 복원 안 함) ===
			.SpawnEntity(Entity_Debris_Glass)
			.SetPosition(Spawned, pos)
			.SaveStoreEntity(Spawned, PropertyId::TerrainTypeId, typeId)
			.SaveConstEntity(Spawned, PropertyId::Health, 1)
			.SaveConstEntity(Spawned, PropertyId::MaxHealth, 1)
			.SaveConstEntity(Spawned, PropertyId::IsGrounded, 0)
			.SaveConstEntity(Spawned, PropertyId::JumpVelZ, JumpVelZ_Fragment2)
			.DispatchEventFrom(Story_Debris_Lifecycle, Spawned)

			// === 파편 3 (시각 전용 — DebrisOrigin 미설정, 지형 복원 안 함) ===
			.SpawnEntity(Entity_Debris_Glass)
			.SetPosition(Spawned, pos)
			.SaveStoreEntity(Spawned, PropertyId::TerrainTypeId, typeId)
			.SaveConstEntity(Spawned, PropertyId::Health, 1)
			.SaveConstEntity(Spawned, PropertyId::MaxHealth, 1)
			.SaveConstEntity(Spawned, PropertyId::IsGrounded, 0)
			.SaveConstEntity(Spawned, PropertyId::JumpVelZ, JumpVelZ_Fragment3)
			.DispatchEventFrom(Story_Debris_Lifecycle, Spawned)

			.Halt()
			.BuildAndRegister();
	}
}
