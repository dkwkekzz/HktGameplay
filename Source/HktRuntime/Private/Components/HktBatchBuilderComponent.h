// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktServerRule.h"
#include "HktBatchBuilderComponent.generated.h"

UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktBatchBuilderComponent : public UActorComponent, public IHktBatchBuilder
{
    GENERATED_BODY()

public:
    UHktBatchBuilderComponent();

    // 프레임 시작 전 초기화
    // MaxTotalPlayers: 서버의 최대 동접자 수 (여유 있게 잡음, 예: 10000)
    virtual void ResetFast(int32 NumGroups, int32 MaxTotalPlayers) override;

    // [핵심] 쓰기 공간 확보 (Lock-Free)
    // 필요한 개수(Count)를 요청하면, 쓰기 시작할 인덱스(StartIndex)를 반환
    virtual int32 ClaimPayloadSlots(int32 Count) override;

    // 읽기 전용 접근자 (Send 단계에서 사용)
    // 전체 배열이 아니라, 실제 유효한 데이터가 있는 범위까지만 TArrayView 등으로 반환하면 더 좋음
    virtual TArrayView<const FHktFrameSendPayload> GetValidPayloads() const override;

    virtual FHktSimulationEvent& CreateOrGetGroupFrameBatch(int32 GroupIndex) override;

	virtual TArray<FHktFrameSendPayload>& GetMutablePayloads() override;

	virtual TArray<int64>& GetMutableNewbieOwners(int32 GroupIndex) override;

private:
    // [기존]
    TArray<FHktSimulationEvent> GroupFrameBatches;
    TArray<TArray<int64>> GroupNewbieOwners;

    // [변경] 2차원 배열 -> 1차원 단일 배열 (Global Buffer)
    TArray<FHktFrameSendPayload> GlobalPayloads;

    // [신규] 현재 프레임에서 할당된 페이로드 개수 추적용 원자 변수
    std::atomic<int32> PayloadWriteOffset;
};
