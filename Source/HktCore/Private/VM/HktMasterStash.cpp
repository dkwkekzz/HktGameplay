// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktMasterStash.h"
#include "HktVMTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

FHktMasterStash::FHktMasterStash()
    : FHktStashBase()
{
    EntityCreationFrame.SetNumZeroed(MaxEntities);
    EntityCells.Init(InvalidCell, MaxEntities);
}

void FHktMasterStash::ApplyWrites(const TArray<FPendingWrite>& Writes)
{
    // 위치 변경된 엔티티 추적
    TSet<FHktEntityId> PositionChangedEntities;

    for (const auto& W : Writes)
    {
        SetProperty(W.Entity, W.PropertyId, W.Value);

        // PosX, PosY, PosZ 중 하나라도 변경되면 셀 업데이트 필요
        if (W.PropertyId == PropertyId::PosX ||
            W.PropertyId == PropertyId::PosY ||
            W.PropertyId == PropertyId::PosZ)
        {
            PositionChangedEntities.Add(W.Entity);
        }
    }

    // 위치 변경된 엔티티들의 셀 업데이트
    for (FHktEntityId Entity : PositionChangedEntities)
    {
        FVector Position;
        if (TryGetPosition(Entity, Position))
        {
            FIntPoint NewCell = PositionToCell(Position);
            UpdateEntityCell(Entity, NewCell);
        }
    }
}

bool FHktMasterStash::ValidateEntityFrame(FHktEntityId Entity, int32 FrameNumber) const
{
    if (!IsValidEntity(Entity))
        return false;
    return EntityCreationFrame[Entity] <= FrameNumber;
}

FHktEntitySnapshot FHktMasterStash::CreateEntitySnapshot(FHktEntityId Entity) const
{
    FHktEntitySnapshot Snapshot;
    
    if (!IsValidEntity(Entity))
        return Snapshot;
    
    Snapshot.EntityId = Entity;
    Snapshot.Properties.SetNum(MaxProperties);
    
    // Property 복사
    for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
    {
        Snapshot.Properties[PropId] = Properties[PropId][Entity.RawValue];
    }
    
    // Tag 복사
    Snapshot.Tags = EntityTags[Entity.RawValue];
    
    return Snapshot;
}

TArray<FHktEntitySnapshot> FHktMasterStash::CreateSnapshots(const TArray<FHktEntityId>& Entities) const
{
    TArray<FHktEntitySnapshot> Snapshots;
    Snapshots.Reserve(Entities.Num());
    
    for (FHktEntityId E : Entities)
    {
        FHktEntitySnapshot Snap = CreateEntitySnapshot(E);
        if (Snap.IsValid())
        {
            Snapshots.Add(MoveTemp(Snap));
        }
    }
    
    return Snapshots;
}

TArray<uint8> FHktMasterStash::SerializeFullState() const
{
    TArray<uint8> Data;
    FMemoryWriter Writer(Data);
    
    int32 Frame = CompletedFrameNumber;
    int32 NextId = NextEntityId;
    Writer << Frame;
    Writer << NextId;
    
    // Valid entities
    int32 NumValid = GetEntityCount();
    Writer << NumValid;
    
    for (int32 E = 0; E < MaxEntities; ++E)
    {
        if (ValidEntities[E])
        {
            int32 EntityInt = E;
            Writer << EntityInt;
            
            // Properties
            for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
            {
                int32 PropValue = Properties[PropId][E];
                Writer << PropValue;
            }
            
            // Tags (FGameplayTagContainer는 자체 직렬화 지원)
            FGameplayTagContainer Tags = EntityTags[E];
            bool bSerializeSuccess = false;
            Tags.NetSerialize(Writer, nullptr, bSerializeSuccess);
        }
    }
    
    return Data;
}

void FHktMasterStash::DeserializeFullState(const TArray<uint8>& Data)
{
    if (Data.Num() == 0)
        return;
    
    FMemoryReader Reader(Data);
    
    int32 Frame, NextId;
    Reader << Frame;
    Reader << NextId;
    
    CompletedFrameNumber = Frame;
    NextEntityId = NextId;
    
    // Clear all
    ValidEntities.Init(false, MaxEntities);
    FreeList.Reset();
    
    int32 NumValid = 0;
    Reader << NumValid;
    
    for (int32 i = 0; i < NumValid; ++i)
    {
        int32 EntityInt;
        Reader << EntityInt;
        
        FHktEntityId E(EntityInt);
        ValidEntities[E.RawValue] = true;
        
        // Properties
        for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
        {
            int32 PropValue;
            Reader << PropValue;
            Properties[PropId][E.RawValue] = PropValue;
        }
        
        // Tags
        bool bSerializeSuccess = false;
        EntityTags[E.RawValue].NetSerialize(Reader, nullptr, bSerializeSuccess);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[MasterStash] Deserialized: Frame=%d, Entities=%d"), 
        CompletedFrameNumber, NumValid);
}

bool FHktMasterStash::TryGetPosition(FHktEntityId Entity, FVector& OutPosition) const
{
    if (!IsValidEntity(Entity))
    {
        return false;
    }
    
    OutPosition.X = static_cast<float>(GetProperty(Entity, PropertyId::PosX));
    OutPosition.Y = static_cast<float>(GetProperty(Entity, PropertyId::PosY));
    OutPosition.Z = static_cast<float>(GetProperty(Entity, PropertyId::PosZ));
    
    return true;
}

void FHktMasterStash::SetPosition(FHktEntityId Entity, const FVector& Position)
{
    if (!IsValidEntity(Entity))
    {
        return;
    }

    // 위치 설정
    SetProperty(Entity, PropertyId::PosX, FMath::RoundToInt(Position.X));
    SetProperty(Entity, PropertyId::PosY, FMath::RoundToInt(Position.Y));
    SetProperty(Entity, PropertyId::PosZ, FMath::RoundToInt(Position.Z));

    // 셀 변경 감지
    FIntPoint NewCell = PositionToCell(Position);
    UpdateEntityCell(Entity, NewCell);
}

uint32 FHktMasterStash::CalculatePartialChecksum(const TArray<FHktEntityId>& Entities) const
{
    uint32 Checksum = 0;
    
    for (FHktEntityId E : Entities)
    {
        if (!IsValidEntity(E))
            continue;
        
        for (int32 PropId = 0; PropId < MaxProperties; ++PropId)
        {
            Checksum ^= Properties[PropId][E.RawValue];
            Checksum = (Checksum << 1) | (Checksum >> 31);
        }
        
        for (const FGameplayTag& Tag : EntityTags[E.RawValue])
        {
            Checksum ^= GetTypeHash(Tag);
            Checksum = (Checksum << 1) | (Checksum >> 31);
        }
        
        Checksum ^= E.RawValue;
    }
    
    return Checksum;
}

void FHktMasterStash::ForEachEntityInRadius(FHktEntityId Center, int32 RadiusCm, TFunctionRef<void(FHktEntityId)> Callback) const
{
    if (!IsValidEntity(Center))
        return;

    int32 CX = GetProperty(Center, PropertyId::PosX);
    int32 CY = GetProperty(Center, PropertyId::PosY);
    int32 CZ = GetProperty(Center, PropertyId::PosZ);
    int64 RadiusSq = static_cast<int64>(RadiusCm) * RadiusCm;

    ForEachEntity([&](FHktEntityId E)
    {
        if (E.RawValue == Center.RawValue) return;

        int32 EX = GetProperty(E, PropertyId::PosX);
        int32 EY = GetProperty(E, PropertyId::PosY);
        int32 EZ = GetProperty(E, PropertyId::PosZ);

        int64 DX = EX - CX;
        int64 DY = EY - CY;
        int64 DZ = EZ - CZ;

        if (DX*DX + DY*DY + DZ*DZ <= RadiusSq)
        {
            Callback(E);
        }
    });
}

// ========== Cell-based Spatial Indexing ==========

FHktEntityId FHktMasterStash::AllocateEntity()
{
    FHktEntityId Entity = FHktStashBase::AllocateEntity();
    if (Entity != InvalidEntityId)
    {
        // 새 엔티티는 아직 위치가 없으므로 InvalidCell
        EntityCells[Entity.RawValue] = InvalidCell;
    }
    return Entity;
}

void FHktMasterStash::FreeEntity(FHktEntityId Entity)
{
    if (IsValidEntity(Entity))
    {
        FIntPoint OldCell = EntityCells[Entity.RawValue];

        // 셀에서 제거
        if (OldCell != InvalidCell)
        {
            if (TSet<FHktEntityId>* CellSet = CellToEntities.Find(OldCell))
            {
                CellSet->Remove(Entity);
                if (CellSet->Num() == 0)
                {
                    CellToEntities.Remove(OldCell);
                }
            }

            // Exit 이벤트 발생
            FHktCellChangeEvent Event;
            Event.Entity = Entity;
            Event.OldCell = OldCell;
            Event.NewCell = InvalidCell;
            PendingCellChangeEvents.Add(Event);
        }

        EntityCells[Entity.RawValue] = InvalidCell;
    }

    FHktStashBase::FreeEntity(Entity);
}

void FHktMasterStash::SetCellSize(float InCellSize)
{
    if (InCellSize > 0.0f && InCellSize != CellSize)
    {
        CellSize = InCellSize;

        // 셀 인덱스 재구축
        CellToEntities.Empty();
        PendingCellChangeEvents.Empty();

        ForEachEntity([this](FHktEntityId Entity)
        {
            FVector Pos;
            if (TryGetPosition(Entity, Pos))
            {
                FIntPoint NewCell = PositionToCell(Pos);
                EntityCells[Entity.RawValue] = NewCell;
                CellToEntities.FindOrAdd(NewCell).Add(Entity);
            }
        });

        UE_LOG(LogTemp, Log, TEXT("[MasterStash] CellSize changed to %.0f, rebuilt spatial index"), CellSize);
    }
}

FIntPoint FHktMasterStash::GetEntityCell(FHktEntityId Entity) const
{
    if (!IsValidEntity(Entity))
    {
        return InvalidCell;
    }
    return EntityCells[Entity.RawValue];
}

const TSet<FHktEntityId>* FHktMasterStash::GetEntitiesInCell(FIntPoint Cell) const
{
    return CellToEntities.Find(Cell);
}

TArray<FHktCellChangeEvent> FHktMasterStash::ConsumeCellChangeEvents()
{
    TArray<FHktCellChangeEvent> Result = MoveTemp(PendingCellChangeEvents);
    PendingCellChangeEvents.Empty();
    return Result;
}

void FHktMasterStash::GetEntitiesInCells(const TSet<FIntPoint>& Cells, TSet<FHktEntityId>& OutEntities) const
{
    for (const FIntPoint& Cell : Cells)
    {
        if (const TSet<FHktEntityId>* CellEntities = CellToEntities.Find(Cell))
        {
            OutEntities.Append(*CellEntities);
        }
    }
}

FIntPoint FHktMasterStash::PositionToCell(const FVector& Position) const
{
    return FIntPoint(
        FMath::FloorToInt(Position.X / CellSize),
        FMath::FloorToInt(Position.Y / CellSize)
    );
}

void FHktMasterStash::UpdateEntityCell(FHktEntityId Entity, FIntPoint NewCell)
{
    FIntPoint OldCell = EntityCells[Entity.RawValue];

    if (OldCell == NewCell)
    {
        return;  // 셀 변경 없음
    }

    // 이전 셀에서 제거
    if (OldCell != InvalidCell)
    {
        if (TSet<FHktEntityId>* CellSet = CellToEntities.Find(OldCell))
        {
            CellSet->Remove(Entity);
            if (CellSet->Num() == 0)
            {
                CellToEntities.Remove(OldCell);
            }
        }
    }

    // 새 셀에 추가
    if (NewCell != InvalidCell)
    {
        CellToEntities.FindOrAdd(NewCell).Add(Entity);
    }

    // 셀 업데이트
    EntityCells[Entity.RawValue] = NewCell;

    // 변경 이벤트 발생
    FHktCellChangeEvent Event;
    Event.Entity = Entity;
    Event.OldCell = OldCell;
    Event.NewCell = NewCell;
    PendingCellChangeEvents.Add(Event);
}
