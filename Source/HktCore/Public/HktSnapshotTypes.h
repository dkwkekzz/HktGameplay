// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "HktEntityTypes.h"
#include "HktEventTypes.h"

namespace Hkt
{
    // ==================================================================================
    // [Part 5] Simulation State Snapshot (외부 공개용)
    // ==================================================================================

    /**
     * @brief [Cache Friendly] 렌더링/외부 모듈을 위한 선형화된 엔티티 데이터
     * - TMap 노드를 순회하지 않고, 이 구조체의 배열을 순회하여 캐시 적중률을 극대화함.
     */
    struct FEntityRenderData
    {
        FEntityID ID;
        FComponentData Data;
    };

    /**
     * @brief [Snapshot] 한 프레임의 완전한 상태
     * - 이 객체는 생성 후 절대 수정되지 않음 (Immutable).
     * - 스마트 포인터로 관리되어 Reader가 있는 동안 메모리가 유지됨.
     */
    struct FHktFrameSnapshot
    {
        uint32 FrameNumber;
        double Timestamp;

        /** Linear Array for Cache Locality (Map -> Array) */
        TArray<FEntityRenderData> Entities;

        /** Events that happened in this frame */
        TArray<FHktEvent> Events;
    };

    using FHktFrameSnapshotConstPtr = TSharedPtr<const FHktFrameSnapshot, ESPMode::ThreadSafe>;
}
