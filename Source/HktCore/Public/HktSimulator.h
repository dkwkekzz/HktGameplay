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
    virtual void RestoreWorldState(const FHktWorldState& InState) = 0;
    virtual void SnapshotWorldState(FHktWorldState& OutState) const = 0;
    virtual void PublishRenderState(FHktRenderState& OutState) = 0;
};

// ============================================================================
// 팩토리 함수
// ============================================================================

/** IHktSimulator 인스턴스 생성 (HktCore 내부 구현) */
HKTCORE_API TUniquePtr<IHktSimulator> CreateSimulationWorld();
