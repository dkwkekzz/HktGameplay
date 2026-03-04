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

	void Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame);
	bool TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame, FVector& CachedTarget);
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
	THktVisualField<int32> AnimState;
	THktVisualField<int32> VisualState;

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
