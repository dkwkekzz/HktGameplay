// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreTypes.h"

// ============================================================================
// FHktWorldState
// ============================================================================

void FHktWorldState::Initialize()
{
    // SOA 인덱스 매핑
    EntityToIndex.Reserve(HktLimits::MaxEntities);
    IndexToEntity.Reserve(HktLimits::MaxEntities);
    FreeIndices.Reserve(HktLimits::MaxEntities);
    TagColumn.Reserve(HktLimits::MaxEntities);
    ActiveEvents.Reserve(HktLimits::MaxActiveEvents);

    // 전체 컬럼 슬롯 사전 활성화 (64 × 4KB = 256KB)
    // PropertyId 하드코딩 없이 [0, MaxProperties) 범위 전체를 즉시 사용 가능
    Columns.SetNum(HktLimits::MaxProperties);
    for (int32 i = 0; i < HktLimits::MaxProperties; ++i)
    {
        FHktDataColumn& Col = Columns[i];
        Col.PropertyId = i;
        Col.IntData.Reserve(HktLimits::MaxEntities);
        Col.DirtyIndices.Reserve(HktLimits::MaxDirtyPerColumn);
    }
}

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
        // 모든 컬럼에 슬롯 확장
        for (FHktDataColumn& Col : Columns)
        {
            Col.IntData.Add(0);
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
    for (FHktDataColumn& Col : Columns)
    {
        Col.SetInt(SlotIndex, 0);
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
    const FHktDataColumn* Col = GetColumn(static_cast<int32>(PropertyId));
    return Col ? Col->GetInt(SlotIndex) : 0;
}

void FHktWorldState::SetProperty(FHktEntityId Entity, uint16 PropertyId, int32 Value)
{
    if (!IsValidEntity(Entity))
        return;
    int32 SlotIndex = EntityToIndex[Entity];
    FHktDataColumn& Col = GetOrCreateColumn(PropertyId);
    Col.SetInt(SlotIndex, Value);
}

FHktDataColumn& FHktWorldState::GetOrCreateColumn(int32 PropertyId)
{
    check(PropertyId >= 0);

    // MaxProperties 범위를 넘으면 Columns 확장 (동적 PropertyId 지원)
    if (PropertyId >= Columns.Num())
    {
        int32 OldNum = Columns.Num();
        Columns.SetNum(PropertyId + 1);
        for (int32 i = OldNum; i < Columns.Num(); ++i)
        {
            Columns[i].PropertyId = i;
            Columns[i].IntData.Reserve(HktLimits::MaxEntities);
            Columns[i].DirtyIndices.Reserve(HktLimits::MaxDirtyPerColumn);
        }
    }

    FHktDataColumn& Col = Columns[PropertyId];
    // Initialize()에서 이미 활성화되었으므로 IntData만 확인
    if (Col.IntData.Num() < IndexToEntity.Num())
    {
        Col.SetZeroed(IndexToEntity.Num());
    }
    return Col;
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

    // Properties — 0이 아닌 값이 있는 최대 PropertyId까지 배열 구성
    int32 MaxPropId = -1;
    for (const FHktDataColumn& Col : Columns)
    {
        if (Col.GetInt(SlotIndex) != 0 && Col.PropertyId > MaxPropId)
            MaxPropId = Col.PropertyId;
    }
    if (MaxPropId >= 0)
    {
        State.Properties.SetNumZeroed(MaxPropId + 1);
        for (const FHktDataColumn& Col : Columns)
        {
            if (Col.PropertyId <= MaxPropId)
            {
                State.Properties[Col.PropertyId] = Col.GetInt(SlotIndex);
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

    // Columns: 데이터가 있는 컬럼만 저장/로드
    if (Ar.IsLoading())
    {
        int32 ColCount;
        Ar << ColCount;

        // 로드 시 필요한 크기만큼 Columns 확보
        int32 RequiredSize = HktLimits::MaxProperties;
        // 첫 패스: 최대 Key를 파악하기 위해 일단 기본 크기로 시작
        WorldState.Columns.SetNum(RequiredSize);
        for (int32 i = 0; i < RequiredSize; ++i)
        {
            WorldState.Columns[i].PropertyId = i;
            WorldState.Columns[i].IntData.Reset();
            WorldState.Columns[i].DirtyIndices.Reset();
        }

        for (int32 i = 0; i < ColCount; ++i)
        {
            int32 Key;
            Ar << Key;
            check(Key >= 0);

            // 동적 범위 확장
            if (Key >= WorldState.Columns.Num())
            {
                int32 OldNum = WorldState.Columns.Num();
                WorldState.Columns.SetNum(Key + 1);
                for (int32 j = OldNum; j < WorldState.Columns.Num(); ++j)
                {
                    WorldState.Columns[j].PropertyId = j;
                }
            }
            Ar << WorldState.Columns[Key];
        }
    }
    else
    {
        // 저장 시 IntData가 있는 컬럼만 기록
        int32 ColCount = 0;
        for (const FHktDataColumn& Col : WorldState.Columns)
        {
            if (Col.IntData.Num() > 0) ++ColCount;
        }
        Ar << ColCount;
        for (FHktDataColumn& Col : WorldState.Columns)
        {
            if (Col.IntData.Num() == 0) continue;
            int32 Key = Col.PropertyId;
            Ar << Key;
            Ar << Col;
        }
    }

    Ar << WorldState.TagColumn;
    Ar << WorldState.ActiveEvents;
    return Ar;
}
