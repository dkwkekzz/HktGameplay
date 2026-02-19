// Copyright Hkt Studios, Inc. All Rights Reserved.
// [Flat SOA Refactor] - FHktWorldState 구현부

#include "HktCoreTypes.h"

// ============================================================================
// FHktWorldState — Flat SOA Implementation
// ============================================================================

void FHktWorldState::Initialize()
{
    NumProperties = HktLimits::MaxProperties;
    MaxSlots = HktLimits::MaxEntities;

    // 단일 연속 버퍼 할당: MaxProperties × MaxEntities = 64 × 1024 = 65536 int32 = 256KB
    FlatData.SetNumZeroed(NumProperties * MaxSlots);

    // 인덱스 매핑
    EntityToIndex.Reserve(HktLimits::MaxEntities);
    IndexToEntity.Reserve(HktLimits::MaxEntities);
    FreeIndices.Reserve(HktLimits::MaxEntities);
    TagColumn.Reserve(HktLimits::MaxEntities);
    ActiveEvents.Reserve(HktLimits::MaxActiveEvents);

    // DirtyTracker
    DirtyTracker.Initialize(NumProperties);
}

// ============================================================================
// 내부: 용량 확장
// ============================================================================

void FHktWorldState::EnsurePropertyCapacity(int32 PropertyId)
{
    if (PropertyId < NumProperties && MaxSlots > 0)
        return;

    // Property 수 확장이 필요한 경우
    if (PropertyId >= NumProperties)
    {
        int32 NewNumProperties = PropertyId + 1;
        int32 EffectiveMaxSlots = FMath::Max(MaxSlots, 1);

        // 새 버퍼 할당 및 기존 데이터 복사
        TArray<int32> NewFlatData;
        NewFlatData.SetNumZeroed(NewNumProperties * EffectiveMaxSlots);

        // 기존 데이터를 새 레이아웃으로 복사
        for (int32 Prop = 0; Prop < NumProperties; ++Prop)
        {
            const int32 OldOffset = Prop * MaxSlots;
            const int32 NewOffset = Prop * EffectiveMaxSlots;
            const int32 CopyCount = FMath::Min(MaxSlots, EffectiveMaxSlots);
            if (CopyCount > 0)
            {
                FMemory::Memcpy(
                    NewFlatData.GetData() + NewOffset,
                    FlatData.GetData() + OldOffset,
                    CopyCount * sizeof(int32));
            }
        }

        FlatData = MoveTemp(NewFlatData);
        MaxSlots = EffectiveMaxSlots;
        NumProperties = NewNumProperties;

        DirtyTracker.EnsurePropertyId(PropertyId);
    }
}

void FHktWorldState::GrowSlots(int32 NewMaxSlots)
{
    if (NewMaxSlots <= MaxSlots)
        return;

    TArray<int32> NewFlatData;
    NewFlatData.SetNumZeroed(NumProperties * NewMaxSlots);

    // 각 Property 컬럼의 기존 데이터를 새 스트라이드로 복사
    for (int32 Prop = 0; Prop < NumProperties; ++Prop)
    {
        const int32 OldOffset = Prop * MaxSlots;
        const int32 NewOffset = Prop * NewMaxSlots;
        if (MaxSlots > 0)
        {
            FMemory::Memcpy(
                NewFlatData.GetData() + NewOffset,
                FlatData.GetData() + OldOffset,
                MaxSlots * sizeof(int32));
        }
    }

    FlatData = MoveTemp(NewFlatData);
    MaxSlots = NewMaxSlots;
}

// ============================================================================
// Entity 할당/제거
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

        // 슬롯 수가 MaxSlots를 넘으면 확장
        if (SlotIndex >= MaxSlots)
        {
            GrowSlots(FMath::Max(MaxSlots * 2, SlotIndex + 1));
        }
    }

    // EntityToIndex 확장
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

    // 슬롯 데이터 초기화 (모든 Property의 해당 슬롯을 0으로)
    for (int32 Prop = 0; Prop < NumProperties; ++Prop)
    {
        FlatData[Prop * MaxSlots + SlotIndex] = 0;
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

// GetProperty와 SetProperty는 헤더에 인라인으로 정의됨

// ============================================================================
// DTO 변환
// ============================================================================

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
    for (int32 Prop = 0; Prop < NumProperties; ++Prop)
    {
        if (FlatData[Prop * MaxSlots + SlotIndex] != 0 && Prop > MaxPropId)
            MaxPropId = Prop;
    }
    if (MaxPropId >= 0)
    {
        State.Properties.SetNumZeroed(MaxPropId + 1);
        for (int32 Prop = 0; Prop <= MaxPropId; ++Prop)
        {
            State.Properties[Prop] = FlatData[Prop * MaxSlots + SlotIndex];
        }
    }

    return State;
}

// ============================================================================
// [EntityStates 복원] Flat SOA에 엔터티 상태 주입
// ============================================================================

void FHktWorldState::RestoreEntities(const TArray<FHktEntityState>& InEntityStates)
{
    for (const FHktEntityState& State : InEntityStates)
    {
        // 1. 엔터티 슬롯 할당
        FHktEntityId NewId = AllocateEntity();
        const int32 SlotIndex = GetIndex(NewId);
        if (SlotIndex == -1) continue;

        // 2. Properties → Flat SOA에 직접 기록
        for (int32 PropIdx = 0; PropIdx < State.Properties.Num(); ++PropIdx)
        {
            if (State.Properties[PropIdx] != 0)
            {
                EnsurePropertyCapacity(PropIdx);
                FlatData[PropIdx * MaxSlots + SlotIndex] = State.Properties[PropIdx];
            }
        }

        // 3. TagColumn 복원
        if (TagColumn.IsValidIndex(SlotIndex))
        {
            TagColumn[SlotIndex] = State.TagIndices;
        }
    }
}

// ============================================================================
// Snapshot / Rollback — memcpy 1회!
// ============================================================================

void FHktWorldState::CopyFrom(const FHktWorldState& Other)
{
    FrameNumber = Other.FrameNumber;
    RandomSeed = Other.RandomSeed;
    NextEntityId = Other.NextEntityId;
    EntityToIndex = Other.EntityToIndex;
    IndexToEntity = Other.IndexToEntity;
    FreeIndices = Other.FreeIndices;

    // [Flat SOA 핵심] 단일 버퍼 복사
    NumProperties = Other.NumProperties;
    MaxSlots = Other.MaxSlots;
    FlatData = Other.FlatData;  // TArray 복사 (내부적으로 memcpy)

    TagColumn = Other.TagColumn;
    ActiveEvents = Other.ActiveEvents;

    // DirtyTracker는 Transient — 복사하지 않고 초기화
    DirtyTracker.Initialize(NumProperties);
}

// ============================================================================
// 직렬화
// ============================================================================

FArchive& operator<<(FArchive& Ar, FHktWorldState& WorldState)
{
    Ar << WorldState.FrameNumber;
    Ar << WorldState.RandomSeed;
    Ar << WorldState.NextEntityId;
    Ar << WorldState.EntityToIndex;
    Ar << WorldState.IndexToEntity;
    Ar << WorldState.FreeIndices;

    // [Flat SOA] 메타데이터 + 데이터
    Ar << WorldState.NumProperties;
    Ar << WorldState.MaxSlots;

    if (Ar.IsLoading())
    {
        WorldState.FlatData.SetNumZeroed(WorldState.NumProperties * WorldState.MaxSlots);
        WorldState.DirtyTracker.Initialize(WorldState.NumProperties);
    }

    // 0이 아닌 데이터가 있는 Property만 직렬화 (대역폭 절약)
    if (Ar.IsSaving())
    {
        // 데이터가 있는 컬럼 수 카운트
        int32 ActiveColCount = 0;
        for (int32 Prop = 0; Prop < WorldState.NumProperties; ++Prop)
        {
            bool bHasData = false;
            const int32 Offset = Prop * WorldState.MaxSlots;
            for (int32 Slot = 0; Slot < WorldState.IndexToEntity.Num(); ++Slot)
            {
                if (WorldState.FlatData[Offset + Slot] != 0)
                {
                    bHasData = true;
                    break;
                }
            }
            if (bHasData) ++ActiveColCount;
        }
        Ar << ActiveColCount;

        for (int32 Prop = 0; Prop < WorldState.NumProperties; ++Prop)
        {
            bool bHasData = false;
            const int32 Offset = Prop * WorldState.MaxSlots;
            for (int32 Slot = 0; Slot < WorldState.IndexToEntity.Num(); ++Slot)
            {
                if (WorldState.FlatData[Offset + Slot] != 0)
                {
                    bHasData = true;
                    break;
                }
            }
            if (!bHasData) continue;

            int32 PropId = Prop;
            Ar << PropId;

            // 유효한 슬롯 수만 직렬화
            int32 SlotCount = WorldState.IndexToEntity.Num();
            Ar << SlotCount;
            Ar.Serialize(
                WorldState.FlatData.GetData() + Offset,
                SlotCount * sizeof(int32));
        }
    }
    else // Loading
    {
        int32 ActiveColCount;
        Ar << ActiveColCount;

        for (int32 i = 0; i < ActiveColCount; ++i)
        {
            int32 PropId;
            Ar << PropId;

            WorldState.EnsurePropertyCapacity(PropId);

            int32 SlotCount;
            Ar << SlotCount;

            const int32 Offset = PropId * WorldState.MaxSlots;
            Ar.Serialize(
                WorldState.FlatData.GetData() + Offset,
                SlotCount * sizeof(int32));
        }
    }

    Ar << WorldState.TagColumn;
    Ar << WorldState.ActiveEvents;
    return Ar;
}
