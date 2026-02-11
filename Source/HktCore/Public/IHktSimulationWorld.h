// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"

// ============================================================================
// IHktSimulationWorld - 결정론적 시뮬레이터 인터페이스
// ============================================================================
class HKTCORE_API IHktSimulationWorld
{
public:
    virtual ~IHktSimulationWorld() = default;

    virtual void ProcessBatch(const FHktSimulationEvent& Event) = 0;    
    virtual void RestoreState(const FHktWorldState& InState) = 0;
    virtual void GetStateSnapshot(FHktWorldState& OutState) const = 0;
    virtual void PublishRenderState(FHktRenderState& OutState) = 0;
    virtual const FHktWorldState& GetWorldState() const = 0;
};

// ============================================================================
// 팩토리 함수
// ============================================================================

/** IHktSimulationWorld 인스턴스 생성 (HktCore 내부 구현) */
HKTCORE_API TUniquePtr<IHktSimulationWorld> CreateSimulationWorld();
