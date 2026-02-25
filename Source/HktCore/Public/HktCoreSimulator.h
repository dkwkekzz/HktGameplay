// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktCoreEvents.h"
#include "HktWorldState.h"

// ============================================================================
// IHktDeterminismSimulator — 클라/서버 공통 코어 시뮬레이터 (내부 시뮬레이션만)
// ============================================================================

class HKTCORE_API IHktDeterminismSimulator
{
public:
    virtual ~IHktDeterminismSimulator() = default;

    virtual FHktSimulationDiff AdvanceFrame(const FHktSimulationEvent& InEvent) = 0;
    virtual const FHktWorldState& GetWorldState() const = 0;
    virtual FHktPlayerState ExportPlayerState(int64 OwnerUid) const = 0;
    virtual void RestoreWorldState(const FHktWorldState& InState) = 0;
};

// ============================================================================
// Factory
// ============================================================================

/** 클라이언트/서버 공통: 결정론 시뮬레이터 (서버는 반환값을 IHktAuthoritySimulator*로 캐스트하여 사용) */
HKTCORE_API TUniquePtr<IHktDeterminismSimulator> CreateDeterminismSimulator();
