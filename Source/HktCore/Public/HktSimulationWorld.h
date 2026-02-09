// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"
#include "HktEntityTypes.h"
#include "HktEventTypes.h"
#include "HktWorldState.h"
#include "HktSpatialSystem.h"
#include "HktVMProcessor.h"
#include "HktSnapshotTypes.h"

namespace Hkt
{
    // ==================================================================================
    // [Part 6] Simulation World (메인 루프)
    // ==================================================================================

    class HKTCORE_API FHktSimulationWorld
    {
    public:
        FHktSimulationWorld();

        void Tick(uint32 FrameNumber, double Time);
        void AddInputEvent(const FHktEvent& Event);

        /**
         * @brief [Thread-Safe] 가장 최근에 완료된 프레임의 스냅샷을 가져옵니다.
         * - Lock 비용이 거의 없으며(포인터 복사), 리턴된 스냅샷은 불변(Immutable)이므로 안전합니다.
         */
        FHktFrameSnapshotConstPtr GetLastSnapshot() const;

    private:
        void PublishSnapshot(uint32 FrameNumber, double Time, const TArray<FHktEvent>& CurrentEvents);

    private:
        FHktSpatialSystem SpatialSystem;
        FHktWorldState WorldState;
        FHktVMProcessor VMProcessor;

        /** Snapshot Management */
        mutable FCriticalSection SnapshotLock;
        FHktFrameSnapshotConstPtr LastCommittedSnapshot;
    };
}
