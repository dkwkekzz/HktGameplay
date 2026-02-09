// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktServerRule.h"
#include "HktBatchBuilderComponent.generated.h"

/**
 * UHktBatchBuilderComponent - IHktBatchBuilder 구현
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - Rule의 OnTick_ExecuteFrame이 이 인터페이스로 배치를 구성
 *   - Rule의 OnTick_SendFrameBatch가 이 인터페이스에서 결과를 읽음
 *
 * 역할:
 *   - RelevancyGroup별 FrameBatch 생성 및 관리
 *   - 신규 유저(Newbie) 목록 및 State 관리
 *   - 매 프레임 Reset으로 재사용
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktBatchBuilderComponent : public UActorComponent, public IHktBatchBuilder
{
    GENERATED_BODY()

public:
    UHktBatchBuilderComponent();

    // === IHktBatchBuilder 구현 ===

    virtual FHktFrameBatch& CreateOrGetGroupFrameBatch(int32 InGroupIdx) override;
    virtual TArray<int64>& GetMutableNewbieOwners(int32 InGroupIdx) override;
    virtual FHktGroupSimulationState& CreateOrGetNewbieState(int32 InGroupIdx) override;

    virtual const FHktFrameBatch& GetGroupFrameBatch(int32 InGroupIdx) const override;
    virtual const TArray<int64>& GetNewbieOwners(int32 InGroupIdx) const override;
    virtual const FHktGroupSimulationState* GetNewbieState(int32 InGroupIdx) const override;

    // === 프레임 관리 ===

    /** 프레임 시작 시 초기화 (그룹 수 지정) */
    void Reset(int32 NumGroups);

private:
    // 그룹별 FrameBatch
    TArray<FHktFrameBatch> GroupBatches;

    // 그룹별 신규 유저 목록
    TArray<TArray<int64>> NewbieOwners;

    // 그룹별 신규 유저용 시뮬레이션 상태
    TMap<int32, FHktGroupSimulationState> NewbieStates;

    // 빈 배열 (const 참조 반환용)
    static const TArray<int64> EmptyOwners;
};
