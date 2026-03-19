// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktVisualField.h"
#include "HktCoreDefs.h"
#include "HktWorldState.h"
#include "HktCoreProperties.h"

/** Transform 그룹 */
struct FHktVM_Transform
{
	THktVisualField<FVector> Location;
	THktVisualField<FRotator> Rotation;

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame, FVector& CachedLoc, FRotator& CachedRot);
};

/** Movement 그룹 */
struct FHktVM_Movement
{
	THktVisualField<FVector> MoveTarget;
	THktVisualField<float> MoveForce;
	THktVisualField<bool> bIsMoving;
	THktVisualField<FVector> Velocity;

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame, FVector& CachedTarget, FVector& CachedVel);
};

/** Health/Mana 그룹 */
struct FHktVM_Vitals
{
	THktVisualField<float> Health;
	THktVisualField<float> MaxHealth;
	THktVisualField<float> HealthRatio;
	THktVisualField<float> Mana;
	THktVisualField<float> MaxMana;
	THktVisualField<float> ManaRatio;

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame);
};

/** Combat 그룹 */
struct FHktVM_Combat
{
	THktVisualField<int32> AttackPower;
	THktVisualField<int32> Defense;

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame);
};

/** Team/Owner 그룹 */
struct FHktVM_Ownership
{
	THktVisualField<int32> Team;
	THktVisualField<int64> OwnedPlayerUid;

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame);
};

/** Animation/Visual 그룹 */
struct FHktVM_Animation
{
	/** FullBody 애니메이션 상태 태그 (Anim.FullBody.Locomotion.Idle 등) */
	THktVisualField<FGameplayTag> AnimState;

	/** 원샷 몽타주 태그 (Anim.Montage.Attack 등) */
	THktVisualField<FGameplayTag> MontageState;

	/** UpperBody 애니메이션 상태 태그 (Anim.UpperBody.Combat.Attack 등) */
	THktVisualField<FGameplayTag> AnimStateUpper;

	/** 현재 Stance — FGameplayTagNetIndex로 저장 (Entity.Stance.Unarmed, Entity.Stance.Spear 등) */
	THktVisualField<int32> Stance;

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame);
};

/** 시각용 대표 태그 (액터/캐릭터 스폰 시 TagDataAsset 로딩 키) */
struct FHktVM_Visualization
{
	THktVisualField<FGameplayTag> VisualElement;

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame);
};

/** 아이템 소켓 부착 그룹 — OwnerEntity + ActionSlot 추적 */
struct FHktVM_Item
{
	THktVisualField<int32> OwnerEntity;   // 소유 캐릭터 EntityId (0 = 없음)
	THktVisualField<int32> ActionSlot;    // -1 = 미등록, 0+ = 장착 슬롯

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame);

	bool IsAttached() const { return OwnerEntity.Get() > 0 && ActionSlot.Get() >= 0; }
};
