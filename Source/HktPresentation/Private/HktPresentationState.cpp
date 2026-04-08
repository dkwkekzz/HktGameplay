// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPresentationState.h"
#include "GameplayTagsManager.h"
#include "HktRuntimeTags.h"

namespace
{
	static FGameplayTag IndexToTag(int32 InTagNetIndex)
	{
		FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(InTagNetIndex));
		return FGameplayTag::RequestGameplayTag(TagName);
	}

	static const FLinearColor GTeamColors[] = {
		FLinearColor::White,
		FLinearColor(0.3f, 0.6f, 1.f),
		FLinearColor(1.f, 0.3f, 0.3f),
		FLinearColor(0.3f, 1.f, 0.3f),
		FLinearColor(1.f, 1.f, 0.3f)
	};
	static constexpr int32 GTeamColorCount = UE_ARRAY_COUNT(GTeamColors);
}

// --------------------------------------------------------------------------- FHktEntityPresentation
void FHktEntityPresentation::InitFromWorldState(const FHktWorldState& WS, FHktEntityId Id, int64 Frame)
{
	EntityId = Id;
	Tags = WS.GetTags(Id);
	TagsDirtyFrame = Frame;
	RenderCategory = DetermineRenderCategory(Tags);
	SpawnedFrame = Frame;
	RemovedFrame = 0;
	LastDirtyFrame = Frame;

	// Transform
	FIntVector P = WS.GetPosition(Id);
	Location.Set(FVector(static_cast<float>(P.X), static_cast<float>(P.Y), static_cast<float>(P.Z)), Frame);
	Rotation.Set(FRotator(0.f, static_cast<float>(WS.GetProperty(Id, PropertyId::RotYaw)), 0.f), Frame);

	// Physics
	CollisionRadius.Set(FMath::Max(static_cast<float>(WS.GetProperty(Id, PropertyId::CollisionRadius)), 50.f), Frame);
	CollisionLayer.Set(WS.GetProperty(Id, PropertyId::CollisionLayer), Frame);

	// Movement
	MoveTarget.Set(FVector(
		static_cast<float>(WS.GetProperty(Id, PropertyId::MoveTargetX)),
		static_cast<float>(WS.GetProperty(Id, PropertyId::MoveTargetY)),
		static_cast<float>(WS.GetProperty(Id, PropertyId::MoveTargetZ))), Frame);
	MoveForce.Set(static_cast<float>(WS.GetProperty(Id, PropertyId::MoveForce)), Frame);
	bIsMoving.Set(WS.GetProperty(Id, PropertyId::IsMoving) != 0, Frame);
	Velocity.Set(FVector(
		static_cast<float>(WS.GetProperty(Id, PropertyId::VelX)),
		static_cast<float>(WS.GetProperty(Id, PropertyId::VelY)),
		static_cast<float>(WS.GetProperty(Id, PropertyId::VelZ))), Frame);

	// Vitals
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

	// Combat
	AttackPower.Set(WS.GetProperty(Id, PropertyId::AttackPower), Frame);
	Defense.Set(WS.GetProperty(Id, PropertyId::Defense), Frame);
	{
		int32 CpVal = WS.GetProperty(Id, PropertyId::CP);
		int32 MaxCpVal = WS.GetProperty(Id, PropertyId::MaxCP);
		CP.Set(CpVal, Frame);
		MaxCP.Set(MaxCpVal, Frame);
		CPRatio.Set((MaxCpVal > 0) ? static_cast<float>(CpVal) / static_cast<float>(MaxCpVal) : 0.f, Frame);
	}
	AttackSpeed.Set(WS.GetProperty(Id, PropertyId::AttackSpeed), Frame);
	{
		int32 MprVal = WS.GetProperty(Id, PropertyId::MotionPlayRate);
		MotionPlayRate.Set(MprVal > 0 ? MprVal : WS.GetProperty(Id, PropertyId::AttackSpeed), Frame);
	}

	// Ownership
	Team.Set(WS.GetProperty(Id, PropertyId::Team), Frame);
	OwnedPlayerUid.Set(WS.GetOwnerUid(Id), Frame);

	// Pre-computed Display
	ComputeOwnerLabel(WS.GetOwnerUid(Id), Frame);
	ComputeTeamColor(WS.GetProperty(Id, PropertyId::Team), Frame);

	// Animation
	AnimState.Set(IndexToTag(WS.GetProperty(Id, PropertyId::AnimState)), Frame);
	MontageState.Set(IndexToTag(WS.GetProperty(Id, PropertyId::VisualState)), Frame);
	AnimStateUpper.Set(IndexToTag(WS.GetProperty(Id, PropertyId::AnimStateUpper)), Frame);
	Stance.Set(IndexToTag(WS.GetProperty(Id, PropertyId::Stance)), Frame);

	// Visualization
	VisualElement.Set(IndexToTag(WS.GetProperty(Id, PropertyId::EntitySpawnTag)), Frame);

	// Item
	OwnerEntity.Set(WS.GetProperty(Id, PropertyId::OwnerEntity), Frame);
	EquipIndex.Set(WS.GetProperty(Id, PropertyId::EquipIndex), Frame);
	ItemState.Set(WS.GetProperty(Id, PropertyId::ItemState), Frame);
	Equippable.Set(WS.GetProperty(Id, PropertyId::Equippable), Frame);
}

void FHktEntityPresentation::ApplyDelta(uint16 PropId, int32 NewValue, int64 Frame)
{
	// Transform
	if      (PropId == PropertyId::PosX)   { Location.Value.X = static_cast<float>(NewValue); Location.Set(Location.Value, Frame); }
	else if (PropId == PropertyId::PosY)   { Location.Value.Y = static_cast<float>(NewValue); Location.Set(Location.Value, Frame); }
	else if (PropId == PropertyId::PosZ)   { Location.Value.Z = static_cast<float>(NewValue); Location.Set(Location.Value, Frame); }
	else if (PropId == PropertyId::RotYaw) { Rotation.Value.Yaw = static_cast<float>(NewValue); Rotation.Set(Rotation.Value, Frame); }

	// Movement
	else if (PropId == PropertyId::MoveTargetX) { MoveTarget.Value.X = static_cast<float>(NewValue); MoveTarget.Set(MoveTarget.Value, Frame); }
	else if (PropId == PropertyId::MoveTargetY) { MoveTarget.Value.Y = static_cast<float>(NewValue); MoveTarget.Set(MoveTarget.Value, Frame); }
	else if (PropId == PropertyId::MoveTargetZ) { MoveTarget.Value.Z = static_cast<float>(NewValue); MoveTarget.Set(MoveTarget.Value, Frame); }
	else if (PropId == PropertyId::MoveForce)   { MoveForce.Set(static_cast<float>(NewValue), Frame); }
	else if (PropId == PropertyId::IsMoving)    { bIsMoving.Set(NewValue != 0, Frame); }
	else if (PropId == PropertyId::VelX)        { Velocity.Value.X = static_cast<float>(NewValue); Velocity.Set(Velocity.Value, Frame); }
	else if (PropId == PropertyId::VelY)        { Velocity.Value.Y = static_cast<float>(NewValue); Velocity.Set(Velocity.Value, Frame); }
	else if (PropId == PropertyId::VelZ)        { Velocity.Value.Z = static_cast<float>(NewValue); Velocity.Set(Velocity.Value, Frame); }

	// Physics
	else if (PropId == PropertyId::CollisionRadius) { CollisionRadius.Set(FMath::Max(static_cast<float>(NewValue), 50.f), Frame); }
	else if (PropId == PropertyId::CollisionLayer)  { CollisionLayer.Set(NewValue, Frame); }

	// Vitals
	else if (PropId == PropertyId::Health)
	{
		Health.Set(static_cast<float>(NewValue), Frame);
		HealthRatio.Set((MaxHealth.Get() > 0.f) ? static_cast<float>(NewValue) / MaxHealth.Get() : 0.f, Frame);
	}
	else if (PropId == PropertyId::MaxHealth)
	{
		MaxHealth.Set(static_cast<float>(NewValue), Frame);
		HealthRatio.Set((NewValue > 0) ? Health.Get() / static_cast<float>(NewValue) : 0.f, Frame);
	}
	else if (PropId == PropertyId::Mana)
	{
		Mana.Set(static_cast<float>(NewValue), Frame);
		ManaRatio.Set((MaxMana.Get() > 0.f) ? static_cast<float>(NewValue) / MaxMana.Get() : 0.f, Frame);
	}
	else if (PropId == PropertyId::MaxMana)
	{
		MaxMana.Set(static_cast<float>(NewValue), Frame);
		ManaRatio.Set((NewValue > 0) ? Mana.Get() / static_cast<float>(NewValue) : 0.f, Frame);
	}

	// Combat
	else if (PropId == PropertyId::AttackPower) { AttackPower.Set(NewValue, Frame); }
	else if (PropId == PropertyId::Defense)     { Defense.Set(NewValue, Frame); }
	else if (PropId == PropertyId::CP)
	{
		CP.Set(NewValue, Frame);
		CPRatio.Set((MaxCP.Get() > 0) ? static_cast<float>(NewValue) / static_cast<float>(MaxCP.Get()) : 0.f, Frame);
	}
	else if (PropId == PropertyId::MaxCP)
	{
		MaxCP.Set(NewValue, Frame);
		CPRatio.Set((NewValue > 0) ? static_cast<float>(CP.Get()) / static_cast<float>(NewValue) : 0.f, Frame);
	}
	else if (PropId == PropertyId::AttackSpeed)     { AttackSpeed.Set(NewValue, Frame); }
	else if (PropId == PropertyId::MotionPlayRate)  { MotionPlayRate.Set(NewValue, Frame); }

	// Ownership
	else if (PropId == PropertyId::Team) { Team.Set(NewValue, Frame); ComputeTeamColor(NewValue, Frame); }

	// Animation
	else if (PropId == PropertyId::AnimState)      { AnimState.Set(IndexToTag(NewValue), Frame); }
	else if (PropId == PropertyId::VisualState)    { MontageState.Set(IndexToTag(NewValue), Frame); }
	else if (PropId == PropertyId::AnimStateUpper) { AnimStateUpper.Set(IndexToTag(NewValue), Frame); }
	else if (PropId == PropertyId::Stance)         { Stance.Set(IndexToTag(NewValue), Frame); }

	// Visualization
	else if (PropId == PropertyId::EntitySpawnTag) { VisualElement.Set(IndexToTag(NewValue), Frame); }

	// Item
	else if (PropId == PropertyId::OwnerEntity) { OwnerEntity.Set(NewValue, Frame); }
	else if (PropId == PropertyId::EquipIndex)  { EquipIndex.Set(NewValue, Frame); }
	else if (PropId == PropertyId::ItemState)   { ItemState.Set(NewValue, Frame); }

	// Voxel Skin
	else if (PropId == PropertyId::VoxelSkinSet)  { VoxelSkinSet.Set(NewValue, Frame); }
	else if (PropId == PropertyId::VoxelPalette)  { VoxelPalette.Set(NewValue, Frame); }
	else if (PropId == PropertyId::Equippable)    { Equippable.Set(NewValue, Frame); }
}

void FHktEntityPresentation::ApplyOwnerDelta(int64 NewOwnerUid, int64 Frame)
{
	OwnedPlayerUid.Set(NewOwnerUid, Frame);
	ComputeOwnerLabel(NewOwnerUid, Frame);
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

void FHktEntityPresentation::ComputeOwnerLabel(int64 Uid, int64 Frame)
{
	OwnerLabel.Set(Uid != 0 ? FString::Printf(TEXT("P:%lld"), Uid) : TEXT("-"), Frame);
}

void FHktEntityPresentation::ComputeTeamColor(int32 TeamIndex, int64 Frame)
{
	TeamColor.Set(GetTeamColor(TeamIndex), Frame);
}

FLinearColor FHktEntityPresentation::GetTeamColor(int32 TeamIndex)
{
	return GTeamColors[FMath::Clamp(TeamIndex, 0, GTeamColorCount - 1)];
}

EHktRenderCategory FHktEntityPresentation::DetermineRenderCategory(const FGameplayTagContainer& Tags)
{
	if (Tags.HasTag(HktGameplayTags::Entity_Character) || Tags.HasTag(HktGameplayTags::Entity_NPC) || Tags.HasTag(HktGameplayTags::Entity_Building))
		return EHktRenderCategory::Actor;
	if (Tags.HasTag(HktGameplayTags::Entity_Projectile))
		return EHktRenderCategory::MassEntity;
	if (Tags.HasTag(HktGameplayTags::Entity_Item))
		return EHktRenderCategory::Actor;
	return EHktRenderCategory::None;
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

void FHktPresentationState::ApplyTagDelta(FHktEntityId Id, const FGameplayTagContainer& NewTags)
{
	if (Id >= Entities.Num() || !ValidMask[Id]) return;
	FHktEntityPresentation& E = Entities[Id];

	if (E.LastDirtyFrame != CurrentFrame)
	{
		E.LastDirtyFrame = CurrentFrame;
		DirtyThisFrame.Add(Id);
	}

	E.Tags = NewTags;
	E.TagsDirtyFrame = CurrentFrame;
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
