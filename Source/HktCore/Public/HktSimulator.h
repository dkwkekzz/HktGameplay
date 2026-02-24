// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktSimulationDiff.h"

// ============================================================================
// IHktAuthoritySimulator — 서버 권위 시뮬레이터 인터페이스
// ============================================================================

class HKTCORE_API IHktAuthoritySimulator
{
public:
    virtual ~IHktAuthoritySimulator() = default;

    virtual FHktSimulationDiff AdvanceFrame(const FHktSimulationEvent& InEvent) = 0;
    virtual FHktPlayerState ExportPlayerState(int64 OwnerHash) const = 0;
    virtual const FHktWorldState& GetWorldState() const = 0;

    /** 클라이언트 복원: 전체 월드 상태 덮어쓰기 */
    virtual void RestoreWorldState(const FHktWorldState& InState) = 0;
};

// ============================================================================
// Factory
// ============================================================================

HKTCORE_API TUniquePtr<IHktAuthoritySimulator> CreateAuthoritySimulator();
