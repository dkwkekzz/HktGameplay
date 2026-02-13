// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreTypes.h"

// ============================================================================
// FHktWorldState
// ============================================================================

FHktEntityId FHktWorldState::AllocateEntity()
{
    FHktEntityId NewId = NextEntityId++;
    int32 SlotIndex;
    if (FreeIndices.Num() > 0)
    {
        SlotIndex = FreeIndices.Pop();
        IndexToEntity[SlotIndex] = NewId;
    }
    else
    {
        SlotIndex = IndexToEntity.Num();
        IndexToEntity.Add(NewId);
        TagColumn.AddDefaulted();
        // 기존 컬럼에 슬롯 확장
        for (auto& Pair : Columns)
        {
            Pair.Value.Data.Add(0);
        }
    }

    // EntityToIndex 확장 (EntityId가 배열 범위를 넘으면)
    if (NewId >= EntityToIndex.Num())
    {
        int32 OldNum = EntityToIndex.Num();
        EntityToIndex.SetNum(NewId + 1);
        for (int32 i = OldNum; i < EntityToIndex.Num(); ++i)
        {
            EntityToIndex[i] = -1;
        }
    }
    EntityToIndex[NewId] = SlotIndex;

    // 슬롯 데이터 초기화
    for (auto& Pair : Columns)
    {
        Pair.Value.Set(SlotIndex, 0);
    }
    TagColumn[SlotIndex].Reset();

    return NewId;
}

void FHktWorldState::RemoveEntity(FHktEntityId Id)
{
    if (!IsValidEntity(Id))
        return;

    int32 SlotIndex = EntityToIndex[Id];
    EntityToIndex[Id] = -1;
    IndexToEntity[SlotIndex] = InvalidEntityId;
    FreeIndices.Add(SlotIndex);
}

int32 FHktWorldState::GetProperty(FHktEntityId Entity, uint16 PropertyId) const
{
    if (!IsValidEntity(Entity))
        return 0;
    int32 SlotIndex = EntityToIndex[Entity];
    const FHktDataColumn* Col = Columns.Find(static_cast<int32>(PropertyId));
    return Col ? Col->Get(SlotIndex) : 0;
}

void FHktWorldState::SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value)
{
    if (!IsValidEntity(Entity))
        return;
    int32 SlotIndex = EntityToIndex[Entity];
    FHktDataColumn& Col = GetOrCreateColumn(PropertyId);
    Col.Set(SlotIndex, Value);
}

FHktDataColumn& FHktWorldState::GetOrCreateColumn(int32 PropertyId)
{
    FHktDataColumn* Existing = Columns.Find(PropertyId);
    if (Existing)
        return *Existing;

    FHktDataColumn& NewCol = Columns.Add(PropertyId);
    NewCol.PropertyId = PropertyId;
    NewCol.SetZeroed(IndexToEntity.Num());
    return NewCol;
}

FHktEntityState FHktWorldState::ExtractEntityState(FHktEntityId Id) const
{
    FHktEntityState State;
    State.EntityId = Id;
    if (!IsValidEntity(Id))
        return State;

    int32 SlotIndex = EntityToIndex[Id];

    // Position 조립
    State.Position.X = static_cast<float>(GetProperty(Id, 0)); // PosX
    State.Position.Y = static_cast<float>(GetProperty(Id, 1)); // PosY
    State.Position.Z = static_cast<float>(GetProperty(Id, 2)); // PosZ

    // TagIndices
    if (TagColumn.IsValidIndex(SlotIndex))
    {
        State.TagIndices = TagColumn[SlotIndex];
    }

    // Properties — 모든 컬럼에서 최대 PropertyId를 찾아 배열 구성
    int32 MaxPropId = -1;
    for (const auto& Pair : Columns)
    {
        if (Pair.Value.Get(SlotIndex) != 0 && Pair.Key > MaxPropId)
            MaxPropId = Pair.Key;
    }
    if (MaxPropId >= 0)
    {
        State.Properties.SetNumZeroed(MaxPropId + 1);
        for (const auto& Pair : Columns)
        {
            if (Pair.Key <= MaxPropId)
            {
                State.Properties[Pair.Key] = Pair.Value.Get(SlotIndex);
            }
        }
    }

    return State;
}

void FHktWorldState::CopyFrom(const FHktWorldState& Other)
{
    FrameNumber = Other.FrameNumber;
    RandomSeed = Other.RandomSeed;
    NextEntityId = Other.NextEntityId;
    EntityToIndex = Other.EntityToIndex;
    IndexToEntity = Other.IndexToEntity;
    FreeIndices = Other.FreeIndices;
    Columns = Other.Columns;
    TagColumn = Other.TagColumn;
    ActiveEvents = Other.ActiveEvents;
}

FArchive& operator<<(FArchive& Ar, FHktWorldState& WorldState)
{
    Ar << WorldState.FrameNumber;
    Ar << WorldState.RandomSeed;
    Ar << WorldState.NextEntityId;
    Ar << WorldState.EntityToIndex;
    Ar << WorldState.IndexToEntity;
    Ar << WorldState.FreeIndices;

    // Columns: TMap<int32, FHktDataColumn>
    int32 ColCount = WorldState.Columns.Num();
    Ar << ColCount;
    if (Ar.IsLoading())
    {
        WorldState.Columns.Reset();
        for (int32 i = 0; i < ColCount; ++i)
        {
            int32 Key;
            Ar << Key;
            FHktDataColumn& Col = WorldState.Columns.Add(Key);
            Ar << Col;
        }
    }
    else
    {
        for (auto& Pair : WorldState.Columns)
        {
            int32 Key = Pair.Key;
            Ar << Key;
            Ar << Pair.Value;
        }
    }

    Ar << WorldState.TagColumn;
    Ar << WorldState.ActiveEvents;
    return Ar;
}
