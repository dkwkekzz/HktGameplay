// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCoreTypes.h"
#include "HktPropertyIds.h"

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

    // 컬럼 배열 초기화 (모든 슬롯 inactive)
    Columns.SetNum(HktLimits::MaxProperties);
    for (FHktDataColumn& Col : Columns)
    {
        Col.PropertyId = -1;
    }

    // 알려진 PropertyId 컬럼 사전 생성
    auto PreCreate = [this](int32 PropId)
    {
        FHktDataColumn& Col = Columns[PropId];
        Col.PropertyId = PropId;
        Col.IntData.Reserve(HktLimits::MaxEntities);
        Col.DirtyIndices.Reserve(HktLimits::MaxDirtyPerColumn);
    };

    // PropertyIds.h의 모든 알려진 속성
    for (int32 Id : { PropertyId::PosX, PropertyId::PosY, PropertyId::PosZ, PropertyId::RotYaw,
                      PropertyId::MoveTargetX, PropertyId::MoveTargetY, PropertyId::MoveTargetZ,
                      PropertyId::MoveSpeed, PropertyId::IsMoving,
                      PropertyId::Health, PropertyId::MaxHealth, PropertyId::AttackPower,
                      PropertyId::Defense, PropertyId::Team, PropertyId::Mana, PropertyId::MaxMana,
                      PropertyId::OwnerEntity, PropertyId::EntityType,
                      PropertyId::TargetPosX, PropertyId::TargetPosY, PropertyId::TargetPosZ,
                      PropertyId::Param0, PropertyId::Param1, PropertyId::Param2, PropertyId::Param3,
                      PropertyId::AnimState, PropertyId::VisualState,
                      PropertyId::OwnerPlayerHash })
    {
        PreCreate(Id);
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
        // 기존 활성 컬럼에 슬롯 확장
        for (FHktDataColumn& Col : Columns)
        {
            if (Col.PropertyId == -1) continue;
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
        if (Col.PropertyId == -1) continue;
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
    check(PropertyId >= 0 && PropertyId < Columns.Num());
    FHktDataColumn& Col = Columns[PropertyId];
    if (Col.PropertyId != -1)
        return Col;

    // 새 컬럼 활성화 (Columns 배열 자체는 재할당 없음)
    Col.PropertyId = PropertyId;
    Col.SetZeroed(IndexToEntity.Num());
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

    // Properties — 모든 활성 컬럼에서 최대 PropertyId를 찾아 배열 구성
    int32 MaxPropId = -1;
    for (const FHktDataColumn& Col : Columns)
    {
        if (Col.PropertyId == -1) continue;
        if (Col.GetInt(SlotIndex) != 0 && Col.PropertyId > MaxPropId)
            MaxPropId = Col.PropertyId;
    }
    if (MaxPropId >= 0)
    {
        State.Properties.SetNumZeroed(MaxPropId + 1);
        for (const FHktDataColumn& Col : Columns)
        {
            if (Col.PropertyId == -1) continue;
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

    // Columns: 활성 컬럼(PropertyId != -1)만 저장/로드
    if (Ar.IsLoading())
    {
        // 로드 시 컬럼 배열 초기화
        WorldState.Columns.SetNum(HktLimits::MaxProperties);
        for (FHktDataColumn& Col : WorldState.Columns)
        {
            Col.PropertyId = -1;
            Col.IntData.Reset();
            Col.DirtyIndices.Reset();
        }

        int32 ColCount;
        Ar << ColCount;
        for (int32 i = 0; i < ColCount; ++i)
        {
            int32 Key;
            Ar << Key;
            check(Key >= 0 && Key < HktLimits::MaxProperties);
            Ar << WorldState.Columns[Key];
        }
    }
    else
    {
        // 저장 시 활성 컬럼만 기록
        int32 ColCount = 0;
        for (const FHktDataColumn& Col : WorldState.Columns)
        {
            if (Col.PropertyId != -1) ++ColCount;
        }
        Ar << ColCount;
        for (FHktDataColumn& Col : WorldState.Columns)
        {
            if (Col.PropertyId == -1) continue;
            int32 Key = Col.PropertyId;
            Ar << Key;
            Ar << Col;
        }
    }

    Ar << WorldState.TagColumn;
    Ar << WorldState.ActiveEvents;
    return Ar;
}
