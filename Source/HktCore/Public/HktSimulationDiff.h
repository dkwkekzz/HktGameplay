// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreMinimal.h"
#include "HktWorldState.h"

// ============================================================================
// FHktPropertyDelta — 단일 프로퍼티 변경
// ============================================================================

struct HKTCORE_API FHktPropertyDelta
{
    FHktEntityId EntityId = InvalidEntityId;
    uint16 PropertyId = 0;
    int32 NewValue = 0;

    friend FArchive& operator<<(FArchive& Ar, FHktPropertyDelta& D)
    {
        Ar << D.EntityId << D.PropertyId << D.NewValue;
        return Ar;
    }
};

// ============================================================================
// FHktSimulationDiff — 프레임별 변경점 (서버 → 클라이언트)
// ============================================================================

struct HKTCORE_API FHktSimulationDiff
{
    int64 FrameNumber = 0;
    TArray<FHktEntityState> SpawnedEntities;
    TArray<FHktEntityId> RemovedEntities;
    TArray<FHktPropertyDelta> PropertyDeltas;

    friend FArchive& operator<<(FArchive& Ar, FHktSimulationDiff& D)
    {
        Ar << D.FrameNumber << D.SpawnedEntities << D.RemovedEntities << D.PropertyDeltas;
        return Ar;
    }
};

// ============================================================================
// FHktPlayerState — 플레이어 단위 상태 (그룹 이동 / DB 저장)
// ============================================================================

struct HKTCORE_API FHktPlayerState
{
    int64 PlayerUid = 0;
    TArray<FHktEntityState> OwnedEntities;
    TArray<FHktEvent> ActiveEvents;

    friend FArchive& operator<<(FArchive& Ar, FHktPlayerState& S)
    {
        Ar << S.PlayerUid << S.OwnedEntities << S.ActiveEvents;
        return Ar;
    }
};
