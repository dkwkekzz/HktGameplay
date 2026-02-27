// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPresentationState.h"
#include "GameplayTagsManager.h"

// --------------------------------------------------------------------------- FHktEntityPresentation
void FHktEntityPresentation::InitFromWorldState(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	EntityId = Id;
	TypeId = WS.GetEntityType(Id);
	RenderCategory = DetermineRenderCategory(TypeId);
	SpawnedFrame = Frame;
	RemovedFrame = 0;
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
	if (Transform.TryApplyDelta(PropId, NewValue, Frame, Transform.Location.Value, Transform.Rotation.Value)) return;
	if (Movement.TryApplyDelta(PropId, NewValue, Frame, Movement.MoveTarget.Value)) return;
	if (Vitals.TryApplyDelta(PropId, NewValue, Frame)) return;
	if (Combat.TryApplyDelta(PropId, NewValue, Frame)) return;
	if (Ownership.TryApplyDelta(PropId, NewValue, Frame)) return;
	if (Animation.TryApplyDelta(PropId, NewValue, Frame)) return;
	if (Visualization.TryApplyDelta(PropId, NewValue, Frame)) return;
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
	bool bWasCleanThisFrame = !IsEntityDirtyThisFrame(E);
	E.ApplyDelta(PropId, NewValue, CurrentFrame);
	if (bWasCleanThisFrame)
		DirtyThisFrame.Add(Id);
}

void FHktPresentationState::ApplyOwnerDelta(FHktEntityId Id, int64 NewOwnerUid)
{
	if (Id >= Entities.Num() || !ValidMask[Id]) return;
	FHktEntityPresentation& E = Entities[Id];
	bool bWasCleanThisFrame = !IsEntityDirtyThisFrame(E);
	E.ApplyOwnerDelta(NewOwnerUid, CurrentFrame);
	if (bWasCleanThisFrame)
		DirtyThisFrame.Add(Id);
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

bool FHktPresentationState::IsEntityDirtyThisFrame(const FHktEntityPresentation& E) const
{
	return E.Transform.Location.IsDirty(CurrentFrame)
		|| E.Transform.Rotation.IsDirty(CurrentFrame)
		|| E.Movement.bIsMoving.IsDirty(CurrentFrame)
		|| E.Vitals.Health.IsDirty(CurrentFrame)
		|| E.Animation.AnimState.IsDirty(CurrentFrame)
		|| E.Combat.AttackPower.IsDirty(CurrentFrame)
		|| E.Ownership.Team.IsDirty(CurrentFrame);
}
