// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreInterfaces.h"
#include "VM/HktMasterStash.h"
#include "VM/HktVisibleStash.h"
#include "VM/HktVMProcessor.h"
#include "State/HktWorldState.h"
#include "Physics/HktSpatialSystem.h"

//=============================================================================
// 기존 팩토리 함수 (레거시 호환)
//=============================================================================

TUniquePtr<IHktVMProcessorInterface> CreateVMProcessor(IHktStashInterface* InStash)
{
    TUniquePtr<FHktVMProcessor> VMProcessor = MakeUnique<FHktVMProcessor>();
    VMProcessor->Initialize(InStash);
    return VMProcessor;
}

TUniquePtr<IHktMasterStashInterface> CreateMasterStash()
{
    return MakeUnique<FHktMasterStash>();
}

TUniquePtr<IHktVisibleStashInterface> CreateVisibleStash()
{
    return MakeUnique<FHktVisibleStash>();
}

//=============================================================================
// FHktWorldStateAdapter 구현
//=============================================================================

FHktWorldStateAdapter::FHktWorldStateAdapter(FHktWorldState* InWorldState, FHktSpatialSystem* InSpatialSystem)
    : WorldState(InWorldState)
    , SpatialSystem(InSpatialSystem)
{
    check(WorldState);
}

// ========== IHktStashInterface → WorldState 위임 ==========

bool FHktWorldStateAdapter::IsValidEntity(FHktEntityId Entity) const
{
    return WorldState->IsValidEntity(Entity);
}

FHktEntityId FHktWorldStateAdapter::AllocateEntity()
{
    FHktEntityId Entity = WorldState->AllocateEntity();
    if (SpatialSystem && Entity.IsValid())
    {
        SpatialSystem->OnEntityAllocated(Entity);
    }
    return Entity;
}

void FHktWorldStateAdapter::FreeEntity(FHktEntityId Entity)
{
    if (SpatialSystem)
    {
        SpatialSystem->OnEntityFreed(Entity);
    }
    WorldState->FreeEntity(Entity);
}

int32 FHktWorldStateAdapter::GetEntityCount() const
{
    return WorldState->GetEntityCount();
}

int32 FHktWorldStateAdapter::GetProperty(FHktEntityId Entity, uint16 PropertyId) const
{
    return WorldState->GetProperty(Entity, PropertyId);
}

void FHktWorldStateAdapter::SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value)
{
    WorldState->SetProperty(Entity, PropertyId, Value);
}

const FGameplayTagContainer& FHktWorldStateAdapter::GetTags(FHktEntityId Entity) const
{
    return WorldState->GetTags(Entity);
}

void FHktWorldStateAdapter::SetTags(FHktEntityId Entity, const FGameplayTagContainer& Tags)
{
    WorldState->SetTags(Entity, Tags);
}

void FHktWorldStateAdapter::AddTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    WorldState->AddTag(Entity, Tag);
}

void FHktWorldStateAdapter::RemoveTag(FHktEntityId Entity, const FGameplayTag& Tag)
{
    WorldState->RemoveTag(Entity, Tag);
}

bool FHktWorldStateAdapter::HasTag(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    return WorldState->HasTag(Entity, Tag);
}

bool FHktWorldStateAdapter::HasTagExact(FHktEntityId Entity, const FGameplayTag& Tag) const
{
    return WorldState->HasTagExact(Entity, Tag);
}

bool FHktWorldStateAdapter::HasAnyTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const
{
    return WorldState->HasAnyTags(Entity, Tags);
}

bool FHktWorldStateAdapter::HasAllTags(FHktEntityId Entity, const FGameplayTagContainer& Tags) const
{
    return WorldState->HasAllTags(Entity, Tags);
}

FGameplayTag FHktWorldStateAdapter::GetFirstTagWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const
{
    return WorldState->GetFirstTagWithParent(Entity, ParentTag);
}

FGameplayTagContainer FHktWorldStateAdapter::GetTagsWithParent(FHktEntityId Entity, const FGameplayTag& ParentTag) const
{
    return WorldState->GetTagsWithParent(Entity, ParentTag);
}

int32 FHktWorldStateAdapter::GetCompletedFrameNumber() const
{
    return WorldState->GetCompletedFrameNumber();
}

void FHktWorldStateAdapter::MarkFrameCompleted(int32 FrameNumber)
{
    WorldState->MarkFrameCompleted(FrameNumber);
}

void FHktWorldStateAdapter::ForEachEntity(TFunctionRef<void(FHktEntityId)> Callback) const
{
    WorldState->ForEachEntity(Callback);
}

uint32 FHktWorldStateAdapter::CalculateChecksum() const
{
    return WorldState->CalculateChecksum();
}

// ========== IHktMasterStashInterface → WorldState 위임 ==========

void FHktWorldStateAdapter::ApplyWrites(const TArray<IHktMasterStashInterface::FPendingWrite>& Writes)
{
    // FPendingWrite 타입 변환: IHktMasterStashInterface::FPendingWrite → FHktWorldState::FPendingWrite
    TArray<FHktWorldState::FPendingWrite> ConvertedWrites;
    ConvertedWrites.Reserve(Writes.Num());
    for (const auto& Write : Writes)
    {
        ConvertedWrites.Add({Write.Entity, Write.PropertyId, Write.Value});
    }
    WorldState->ApplyWrites(ConvertedWrites);
}

bool FHktWorldStateAdapter::ValidateEntityFrame(FHktEntityId Entity, int32 FrameNumber) const
{
    return WorldState->ValidateEntityFrame(Entity, FrameNumber);
}

FHktEntitySnapshot FHktWorldStateAdapter::CreateEntitySnapshot(FHktEntityId Entity) const
{
    return WorldState->CreateEntitySnapshot(Entity);
}

TArray<FHktEntitySnapshot> FHktWorldStateAdapter::CreateSnapshots(const TArray<FHktEntityId>& Entities) const
{
    return WorldState->CreateSnapshots(Entities);
}

TArray<uint8> FHktWorldStateAdapter::SerializeFullState() const
{
    return WorldState->SerializeFullState();
}

void FHktWorldStateAdapter::DeserializeFullState(const TArray<uint8>& Data)
{
    WorldState->DeserializeFullState(Data);
}

bool FHktWorldStateAdapter::TryGetPosition(FHktEntityId Entity, FVector& OutPosition) const
{
    return WorldState->TryGetPosition(Entity, OutPosition);
}

void FHktWorldStateAdapter::SetPosition(FHktEntityId Entity, const FVector& Position)
{
    WorldState->SetPosition(Entity, Position);
}

uint32 FHktWorldStateAdapter::CalculatePartialChecksum(const TArray<FHktEntityId>& Entities) const
{
    return WorldState->CalculatePartialChecksum(Entities);
}

void FHktWorldStateAdapter::ForEachEntityInRadius(FHktEntityId Center, int32 RadiusCm, TFunctionRef<void(FHktEntityId)> Callback) const
{
    WorldState->ForEachEntityInRadius(Center, RadiusCm, Callback);
}

// ========== Cell API → SpatialSystem 위임 ==========

void FHktWorldStateAdapter::SetCellSize(float InCellSize)
{
    if (SpatialSystem)
    {
        SpatialSystem->SetCellSize(InCellSize);
    }
}

float FHktWorldStateAdapter::GetCellSize() const
{
    return SpatialSystem ? SpatialSystem->GetCellSize() : 5000.0f;
}

FIntPoint FHktWorldStateAdapter::GetEntityCell(FHktEntityId Entity) const
{
    return SpatialSystem ? SpatialSystem->GetEntityCell(Entity) : InvalidCell;
}

const TSet<FHktEntityId>* FHktWorldStateAdapter::GetEntitiesInCell(FIntPoint Cell) const
{
    return SpatialSystem ? SpatialSystem->GetEntitiesInCell(Cell) : nullptr;
}

TArray<FHktCellChangeEvent> FHktWorldStateAdapter::ConsumeCellChangeEvents()
{
    return SpatialSystem ? SpatialSystem->ConsumeCellChangeEvents() : TArray<FHktCellChangeEvent>();
}

void FHktWorldStateAdapter::GetEntitiesInCells(const TSet<FIntPoint>& Cells, TSet<FHktEntityId>& OutEntities) const
{
    if (SpatialSystem)
    {
        SpatialSystem->GetEntitiesInCells(Cells, OutEntities);
    }
}
