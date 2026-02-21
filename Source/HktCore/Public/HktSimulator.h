// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"

// ============================================================================
// IHktSimulator - 결정론적 시뮬레이터 인터페이스
// ============================================================================
class HKTCORE_API IHktSimulator
{
public:
    virtual ~IHktSimulator() = default;

    virtual void ProcessBatch(const FHktSimulationEvent& Event) = 0;
    virtual void ProcessBatches(TArrayView<const FHktSimulationEvent> Events) = 0;
    virtual void RestoreWorldState(const FHktWorldState& InState) = 0;
    virtual void SnapshotWorldState(FHktWorldState& OutState) const = 0;
    virtual void CreateWorldView(FHktWorldView& OutView) = 0;
};

// ============================================================================
// 팩토리 함수
// ============================================================================

/** IHktSimulator 인스턴스 생성 (HktCore 내부 구현) */
HKTCORE_API TUniquePtr<IHktSimulator> CreateSimulationWorld();

struct FHktPropertyDiff
{
    FHktEntityId EntityId;
    int32 PropertyId;
    int32 OldValue;
    int32 NewValue;
};

struct FHktSimulationDiff
{
    TArray<FHktEntityId> CreatedEntities;
    TArray<FHktEntityId> DestroyedEntities;
	TArray<FHktPropertyDiff> UpdatedProperties;
};

// ============================================================================
// IHktAuthoritySimulator - 결정론적 시뮬레이터 인터페이스
// ============================================================================
class HKTCORE_API IHktAuthoritySimulator
{
public:
    virtual ~IHktAuthoritySimulator() = default;

    virtual FHktSimulationDiff AdvanceFrame(const FHktSimulationEvent& InEvent) = 0;
    virtual void ExportPlayerState(FHktPlayerState& OutState) = 0;
    virtual void ApplyPlayerState(const FHktPlayerState& InState) = 0;
    virtual const FHktWorldState& GetWorldState() const = 0;
};

HKTCORE_API TUniquePtr<IHktAuthoritySimulator> CreateAuthoritySimulator();