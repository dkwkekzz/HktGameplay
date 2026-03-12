// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPresentationState.h"
#include "GameplayTagsManager.h"
#include "HktCoreProperties.h"

// --------------------------------------------------------------------------- FHktEntityPresentation
void FHktEntityPresentation::InitFromWorldState(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	EntityId = Id;
	TypeId = WS.GetEntityType(Id);
	RenderCategory = DetermineRenderCategory(TypeId);
	SpawnedFrame = Frame;
	RemovedFrame = 0;
	LastDirtyFrame = Frame; // ���� ������ ������ Dirty ����
	Transform.Apply(WS, Id, Frame);
	Movement.Apply(WS, Id, Frame);
	Vitals.Apply(WS, Id, Frame);
	Combat.Apply(WS, Id, Frame);
	Ownership.Apply(WS, Id, Frame);
	Animation.Apply(WS, Id, Frame);
	Visualization.Apply(WS, Id, Frame);
}

void FHktEntityPresentation::ApplyDelta(uint16 PropId, int32 NewValue, int64 Frame)
{
	// ����ȭ: ĳ�����̵� ���(if-else ü��) ��� ���� Switch������ ���� �����
	switch (PropId)
	{
		// Transform
	case PropertyId::PosX:
	case PropertyId::PosY:
	case PropertyId::PosZ:
	case PropertyId::RotYaw:
		Transform.TryApplyDelta(PropId, NewValue, Frame, Transform.Location.Value, Transform.Rotation.Value);
		break;

		// Movement
	case PropertyId::MoveTargetX:
	case PropertyId::MoveTargetY:
	case PropertyId::MoveTargetZ:
	case PropertyId::MoveForce:
	case PropertyId::IsMoving:
	case PropertyId::MoveSpeed:
		Movement.TryApplyDelta(PropId, NewValue, Frame, Movement.MoveTarget.Value);
		break;

		// Vitals
	case PropertyId::Health:
	case PropertyId::MaxHealth:
	case PropertyId::Mana:
	case PropertyId::MaxMana:
		Vitals.TryApplyDelta(PropId, NewValue, Frame);
		break;

		// Combat
	case PropertyId::AttackPower:
	case PropertyId::Defense:
		Combat.TryApplyDelta(PropId, NewValue, Frame);
		break;

		// Ownership
	case PropertyId::Team:
		Ownership.TryApplyDelta(PropId, NewValue, Frame);
		break;

		// Animation
	case PropertyId::AnimState:
	case PropertyId::VisualState:
	case PropertyId::AnimStateUpper:
		Animation.TryApplyDelta(PropId, NewValue, Frame);
		break;

		// Visualization
	case PropertyId::EntitySpawnTag:
		Visualization.TryApplyDelta(PropId, NewValue, Frame);
		break;
	}
}

void FHktEntityPresentation::ApplyOwnerDelta(int64 NewOwnerUid, int64 Frame)
{
	Ownership.OwnedPlayerUid.Set(NewOwnerUid, Frame);
}

bool FHktEntityPresentation::IsAlive() const
{
	return RemovedFrame == 0;
}

bool FHktEntityPresentation::IsSpawnedAt(int64 Frame) const
{
	return SpawnedFrame == Frame;
}

bool FHktEntityPresentation::IsRemovedAt(int64 Frame) const
{
	return RemovedFrame == Frame;
}

EHktRenderCategory FHktEntityPresentation::DetermineRenderCategory(FHktTypeId Type)
{
	switch (Type)
	{
	case HktType::Unit:       return EHktRenderCategory::Actor;
	case HktType::Building:   return EHktRenderCategory::Actor;
	case HktType::Projectile: return EHktRenderCategory::MassEntity;
	default:                  return EHktRenderCategory::None;
	}
}

// --------------------------------------------------------------------------- FHktPresentationState
void FHktPresentationState::BeginFrame(int64 Frame)
{
	CurrentFrame = Frame;
	SpawnedThisFrame.Reset();
	RemovedThisFrame.Reset();
	DirtyThisFrame.Reset();
}

void FHktPresentationState::EnsureCapacity(FHktEntityId MaxId)
{
	if (MaxId >= Entities.Num())
	{
		int32 OldNum = Entities.Num();
		Entities.SetNum(MaxId + 1);
		ValidMask.SetNumUninitialized(MaxId + 1);
		for (int32 i = OldNum; i <= MaxId; ++i)
			ValidMask[i] = false;
	}
}

void FHktPresentationState::AddEntity(const FHktWorldState& WS, FHktEntityId Id)
{
	EnsureCapacity(Id);
	Entities[Id].InitFromWorldState(WS, Id, CurrentFrame);
	ValidMask[Id] = true;
	SpawnedThisFrame.Add(Id);
}

void FHktPresentationState::RemoveEntity(FHktEntityId Id)
{
	if (Id < Entities.Num() && ValidMask[Id])
	{
		Entities[Id].RemovedFrame = CurrentFrame;
		ValidMask[Id] = false;
		RemovedThisFrame.Add(Id);
	}
}

void FHktPresentationState::ApplyDelta(FHktEntityId Id, uint16 PropId, int32 NewValue)
{
	if (Id >= Entities.Num() || !ValidMask[Id]) return;
	FHktEntityPresentation& E = Entities[Id];

	// ����ȭ: �� �� ���� ���� �񱳷� Dirty ���� ���� �� �ߺ� ���� ����
	if (E.LastDirtyFrame != CurrentFrame)
	{
		E.LastDirtyFrame = CurrentFrame;
		DirtyThisFrame.Add(Id);
	}

	E.ApplyDelta(PropId, NewValue, CurrentFrame);
}

void FHktPresentationState::ApplyOwnerDelta(FHktEntityId Id, int64 NewOwnerUid)
{
	if (Id >= Entities.Num() || !ValidMask[Id]) return;
	FHktEntityPresentation& E = Entities[Id];

	if (E.LastDirtyFrame != CurrentFrame)
	{
		E.LastDirtyFrame = CurrentFrame;
		DirtyThisFrame.Add(Id);
	}

	E.ApplyOwnerDelta(NewOwnerUid, CurrentFrame);
}

bool FHktPresentationState::IsValid(FHktEntityId Id) const
{
	return Id >= 0 && Id < Entities.Num() && ValidMask[Id];
}

const FHktEntityPresentation* FHktPresentationState::Get(FHktEntityId Id) const
{
	return IsValid(Id) ? &Entities[Id] : nullptr;
}

FHktEntityPresentation* FHktPresentationState::GetMutable(FHktEntityId Id)
{
	return IsValid(Id) ? &Entities[Id] : nullptr;
}

int64 FHktPresentationState::GetCurrentFrame() const
{
	return CurrentFrame;
}

void FHktPresentationState::Clear()
{
	Entities.Reset();
	ValidMask.Reset();
	SpawnedThisFrame.Reset();
	RemovedThisFrame.Reset();
	DirtyThisFrame.Reset();
	CurrentFrame = 0;
}
