// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktVMTypes.h"

// Forward Declarations
class FHktVMInterpreter;
class FHktVMRuntimePool;
struct FHktVMStore;

/** 1. Entity Arrange System: 제거된 소유자 정리 */
struct HKTCORE_API FHktEntityArrangeSystem
{
    void Process(FHktWorldState& WorldState, const TArray<int64>& RemovedOwnerIds);
};

/** 2. VM Build System: 이벤트 -> VM 생성 */
struct HKTCORE_API FHktVMBuildSystem
{
    void Process(
        const TArray<FHktEvent>& Events,
        int32 CurrentFrame,
        FHktVMRuntimePool& Pool,
        TArray<FHktVMHandle>& OutActiveVMs,
        const FHktWorldState& WorldState,
        TArray<FHktVMStore>& StorePool
    );
};

/** 3. VM Process System: 바이트코드 실행 */
struct HKTCORE_API FHktVMProcessSystem
{
    FHktVMInterpreter* Interpreter = nullptr;

    void Process(
        TArray<FHktVMHandle>& ActiveVMs,
        TArray<FHktVMHandle>& OutCompletedVMs,
        FHktVMRuntimePool& Pool,
        float DeltaSeconds,
        TArray<FHktPendingEvent>& PendingExternalEvents
    );
};

/** 4. Physics System: 공간 분할 및 충돌 감지 */
struct HKTCORE_API FHktPhysicsSystem
{
    static constexpr float CellSize = 1000.0f;

    struct FCellCoord
    {
        int32 X, Y;
        bool operator==(const FCellCoord& Other) const { return X == Other.X && Y == Other.Y; }
        friend uint32 GetTypeHash(const FCellCoord& C) { return HashCombine(GetTypeHash(C.X), GetTypeHash(C.Y)); }
    };

    TMap<FCellCoord, TArray<FHktEntityId>> GridMap;

    static FCellCoord WorldToCell(const FVector& Pos);
    void RebuildGrid(const FHktWorldState& WorldState);

    void Process(
        FHktWorldState& WorldState,
        TArray<FHktPhysicsEvent>& OutPhysicsEvents
    );
};

/** 5. Apply Store System: 변경 사항(Store) 커밋 */
struct HKTCORE_API FHktApplyStoreSystem
{
    void Process(
        FHktWorldState& WorldState,
        const TArray<FHktVMHandle>& ActiveVMs,
        FHktVMRuntimePool& Pool
    );
};

/** 6. VM Cleanup System: 종료된 VM 해제 */
struct HKTCORE_API FHktVMCleanupSystem
{
    void Process(TArray<FHktVMHandle>& CompletedVMs, FHktVMRuntimePool& Pool);
};

/** 7. Publish System: 렌더링 상태 발행 */
struct HKTCORE_API FHktPublishRenderSystem
{
    void Process(const FHktWorldState& WorldState, FHktRenderState& OutRenderState);
};
