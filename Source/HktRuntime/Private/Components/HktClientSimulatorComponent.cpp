// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktClientSimulatorComponent.h"
#include "HktRuntimeConverter.h"

UHktClientSimulatorComponent::UHktClientSimulatorComponent() 
{ 
    PrimaryComponentTick.bCanEverTick = false; 
}

void UHktClientSimulatorComponent::BeginPlay() 
{ 
    Super::BeginPlay();

    // CoreSimulator 초기화 (BeginPlay에서 생성)
    CoreSimulator = CreateSimulationWorld();
}

void UHktClientSimulatorComponent::Execute(const FHktSimulationEvent& InBatch)
{
    if (!CoreSimulator)
    {
        return;
    }

    // Runtime 타입 -> Core 타입 변환
    // 결정론적 시뮬레이션 실행
    CoreSimulator->ProcessBatch(InBatch);

    // Core 상태 -> Runtime 상태로 역변환하여 캐시
    State = HktRuntimeConverter::ConvertWorldState(CoreSimulator->GetWorldState());

    bInitialized = true;
}

void UHktClientSimulatorComponent::RestoreState(const FHktWorldState& InState, TArray<FHktSimulationEvent>&& InPendingBatches)
{
    if (!CoreSimulator)
    {
        return;
    }

    CoreSimulator->RestoreState(InState);

    // 캐시된 Runtime 상태도 갱신
    State = InState;
    bInitialized = true;

    // 대기 중이던 배치들 중 복원 이후 프레임만 재생
    for (const FHktSimulationEvent& Event : InPendingBatches)
    {
        if (Event.FrameNumber > State.FrameNumber)
        {
            Execute(Event);
        }
    }
}

FHktRuntimeOwnerState UHktClientSimulatorComponent::GetOwnerState(int64 InOwnerId) const
{
    FHktRuntimeOwnerState OwnerState;

    if (!CoreSimulator)
    {
        return OwnerState;
    }

    const FHktWorldState& WorldState = CoreSimulator->GetWorldState();
    for (const auto& Pair : WorldState.Entities)
    {
        // OwnerPlayerHash가 일치하는 엔티티만 포함
        if (Pair.Value.GetProperty(PropertyId::OwnerPlayerHash) == static_cast<int32>(InOwnerId))
        {
            OwnerState.EntityStates.Add(Pair.Value);
        }
    }

    return OwnerState;
}
