// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPresentationViewModels.h"

// --------------------------------------------------------------------------- FHktVM_Transform
void FHktVM_Transform::Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	FIntVector P = WS.GetPosition(Id);
	Location.Set(FVector(static_cast<float>(P.X), static_cast<float>(P.Y), static_cast<float>(P.Z)), Frame);
	Rotation.Set(FRotator(0.f, static_cast<float>(WS.GetProperty(Id, PropertyId::RotYaw)), 0.f), Frame);
}

bool FHktVM_Transform::TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame, FVector& CachedLoc, FRotator& CachedRot)
{
	switch (PropId)
	{
	case PropertyId::PosX:   CachedLoc.X = static_cast<float>(NewValue); Location.Set(CachedLoc, Frame); return true;
	case PropertyId::PosY:   CachedLoc.Y = static_cast<float>(NewValue); Location.Set(CachedLoc, Frame); return true;
	case PropertyId::PosZ:   CachedLoc.Z = static_cast<float>(NewValue); Location.Set(CachedLoc, Frame); return true;
	case PropertyId::RotYaw: CachedRot.Yaw = static_cast<float>(NewValue); Rotation.Set(CachedRot, Frame); return true;
	default: return false;
	}
}

// --------------------------------------------------------------------------- FHktVM_Movement
void FHktVM_Movement::Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	MoveTarget.Set(FVector(
		static_cast<float>(WS.GetProperty(Id, PropertyId::MoveTargetX)),
		static_cast<float>(WS.GetProperty(Id, PropertyId::MoveTargetY)),
		static_cast<float>(WS.GetProperty(Id, PropertyId::MoveTargetZ))), Frame);
	MoveSpeed.Set(static_cast<float>(WS.GetProperty(Id, PropertyId::MoveSpeed)), Frame);
	bIsMoving.Set(WS.GetProperty(Id, PropertyId::IsMoving) != 0, Frame);
}

bool FHktVM_Movement::TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame, FVector& CachedTarget)
{
	switch (PropId)
	{
	case PropertyId::MoveTargetX: CachedTarget.X = static_cast<float>(NewValue); MoveTarget.Set(CachedTarget, Frame); return true;
	case PropertyId::MoveTargetY: CachedTarget.Y = static_cast<float>(NewValue); MoveTarget.Set(CachedTarget, Frame); return true;
	case PropertyId::MoveTargetZ: CachedTarget.Z = static_cast<float>(NewValue); MoveTarget.Set(CachedTarget, Frame); return true;
	case PropertyId::MoveSpeed:   MoveSpeed.Set(static_cast<float>(NewValue), Frame); return true;
	case PropertyId::IsMoving:    bIsMoving.Set(NewValue != 0, Frame); return true;
	default: return false;
	}
}

// --------------------------------------------------------------------------- FHktVM_Vitals
void FHktVM_Vitals::Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	float H = static_cast<float>(WS.GetProperty(Id, PropertyId::Health));
	float MH = static_cast<float>(WS.GetProperty(Id, PropertyId::MaxHealth));
	float M = static_cast<float>(WS.GetProperty(Id, PropertyId::Mana));
	float MM = static_cast<float>(WS.GetProperty(Id, PropertyId::MaxMana));
	Health.Set(H, Frame);
	MaxHealth.Set(MH, Frame);
	HealthRatio.Set((MH > 0.f) ? H / MH : 0.f, Frame);
	Mana.Set(M, Frame);
	MaxMana.Set(MM, Frame);
	ManaRatio.Set((MM > 0.f) ? M / MM : 0.f, Frame);
}

bool FHktVM_Vitals::TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame)
{
	switch (PropId)
	{
	case PropertyId::Health:
		Health.Set(static_cast<float>(NewValue), Frame);
		HealthRatio.Set((MaxHealth.Get() > 0.f) ? static_cast<float>(NewValue) / MaxHealth.Get() : 0.f, Frame);
		return true;
	case PropertyId::MaxHealth:
		MaxHealth.Set(static_cast<float>(NewValue), Frame);
		HealthRatio.Set((NewValue > 0) ? Health.Get() / static_cast<float>(NewValue) : 0.f, Frame);
		return true;
	case PropertyId::Mana:
		Mana.Set(static_cast<float>(NewValue), Frame);
		ManaRatio.Set((MaxMana.Get() > 0.f) ? static_cast<float>(NewValue) / MaxMana.Get() : 0.f, Frame);
		return true;
	case PropertyId::MaxMana:
		MaxMana.Set(static_cast<float>(NewValue), Frame);
		ManaRatio.Set((NewValue > 0) ? Mana.Get() / static_cast<float>(NewValue) : 0.f, Frame);
		return true;
	default: return false;
	}
}

// --------------------------------------------------------------------------- FHktVM_Combat
void FHktVM_Combat::Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	AttackPower.Set(WS.GetProperty(Id, PropertyId::AttackPower), Frame);
	Defense.Set(WS.GetProperty(Id, PropertyId::Defense), Frame);
}

bool FHktVM_Combat::TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame)
{
	switch (PropId)
	{
	case PropertyId::AttackPower: AttackPower.Set(NewValue, Frame); return true;
	case PropertyId::Defense:     Defense.Set(NewValue, Frame); return true;
	default: return false;
	}
}

// --------------------------------------------------------------------------- FHktVM_Ownership
void FHktVM_Ownership::Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	Team.Set(WS.GetProperty(Id, PropertyId::Team), Frame);
	OwnedPlayerUid.Set(static_cast<int64>(WS.GetProperty(Id, PropertyId::OwnedPlayerUid)), Frame);
}

bool FHktVM_Ownership::TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame)
{
	switch (PropId)
	{
	case PropertyId::Team:           Team.Set(NewValue, Frame); return true;
	case PropertyId::OwnedPlayerUid: OwnedPlayerUid.Set(static_cast<int64>(NewValue), Frame); return true;
	default: return false;
	}
}

// --------------------------------------------------------------------------- FHktVM_Animation
void FHktVM_Animation::Apply(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	AnimState.Set(WS.GetProperty(Id, PropertyId::AnimState), Frame);
	VisualState.Set(WS.GetProperty(Id, PropertyId::VisualState), Frame);
}

bool FHktVM_Animation::TryApplyDelta(uint16 PropId, int32 NewValue, int64 Frame)
{
	switch (PropId)
	{
	case PropertyId::AnimState:   AnimState.Set(NewValue, Frame); return true;
	case PropertyId::VisualState: VisualState.Set(NewValue, Frame); return true;
	default: return false;
	}
}
