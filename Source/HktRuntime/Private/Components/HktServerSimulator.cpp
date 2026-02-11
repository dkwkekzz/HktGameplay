// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktServerSimulator.h"
#include "HktSimulationConverters.h"

FHktServerSimulator::FHktServerSimulator()
{
    SimWorld = CreateSimulationWorld();
}

void FHktServerSimulator::Execute(const FHktRuntimeBatch& InBatch)
{
    // Runtime 타입 -> Core 타입 변환
    FHktSimulationEvent SimEvent = HktSimulationConverters::ConvertBatch(InBatch);

    // 결정론적 시뮬레이션 실행
    SimWorld->ProcessBatch(SimEvent);

    // Core 상태 -> Runtime 상태로 역변환하여 캐시
    State = HktSimulationConverters::ConvertWorldState(SimWorld->GetWorldState());

    bInitialized = true;
}

void FHktServerSimulator::RestoreState(const FHktRuntimeSimulationState& InState, TArray<FHktRuntimeBatch>&& InPendingBatches)
{
    // Runtime 상태 -> Core WorldState로 변환하여 복원
    FHktWorldState RestoredWorldState = HktSimulationConverters::ConvertToWorldState(InState);
    SimWorld->RestoreState(RestoredWorldState);

    // 캐시된 Runtime 상태도 갱신
    State = InState;
    bInitialized = true;

    // 대기 중이던 배치들을 순서대로 재생
    for (const FHktRuntimeBatch& Batch : InPendingBatches)
    {
        Execute(Batch);
    }
}

FHktRuntimeOwnerState FHktServerSimulator::GetOwnerState(int64 InOwnerId) const
{
    FHktRuntimeOwnerState OwnerState;

    // TODO( dkwkekzz ): 좀 더 똑똑하게...
    const FHktWorldState& WorldState = SimWorld->GetWorldState();
    for (const auto& Pair : WorldState.Entities)
    {
        // OwnerPlayerHash가 일치하는 엔티티만 포함
        if (Pair.Value.GetProperty(PropertyId::OwnerPlayerHash) == static_cast<int32>(InOwnerId))
        {
            FHktEntitySnapshot Snap;
            Snap.EntityId   = Pair.Key;
            Snap.Properties = Pair.Value.Properties;
            Snap.Tags       = Pair.Value.Tags;
            OwnerState.EntitySnapshots.Add(MoveTemp(Snap));
        }
    }

    return OwnerState;
}
