// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HktServerRuleInterfaces.h"
#include <atomic>
#include "HktSimulationEventBuilderComponent.generated.h"

/**
 * UHktSimulationEventBuilderComponent - IHktSimulationEventBuilder 구현
 *
 * 아키텍처:
 *   - Intent 수집(재료)과 Batch 조립(묶기)을 단일 컴포넌트로 처리
 *   - Actor(GameMode)는 이 컴포넌트를 Rule에 IHktSimulationEventBuilder로 전달
 *
 * 역할:
 *   - 그룹별 Intent 큐 관리 (PushIntents → GetIntents)
 *   - 플레이어 진입/퇴장 큐 관리 (EnterWorldPlayer / ExitWorldPlayer)
 *   - 그룹별 SimulationEvent 조립 (CreateOrGetGroupFrameBatch)
 *   - Lock-Free Payload 할당 (ClaimPayloadSlots)
 *   - 프레임 종료 시 소비된 데이터 정리 (EndFrame)
 *
 * 스레드 안전성:
 *   - ClaimPayloadSlots: std::atomic fetch_add (Lock-Free)
 *   - GetIntents, EndFrame은 메인 스레드에서만 호출
 */
UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktSimulationEventBuilderComponent : public UActorComponent, public IHktSimulationEventBuilder
{
    GENERATED_BODY()

public:
    UHktSimulationEventBuilderComponent();

    // === IHktSimulationEventBuilder 구현 ===

    // --- 프레임 초기화/정리 ---
    virtual void ResetFast(int32 NumGroups, int32 MaxTotalPlayers) override;
    virtual void EndFrame() override;

    // --- Intent 입력 ---
    virtual void PushIntents(int32 GroupIndex, const TArray<FHktEvent>& InEvents) override;
    virtual bool GetIntents(int32 GroupIndex, TArray<FHktEvent>& OutIntents) override;

    // --- 플레이어 진입/퇴장 ---
    virtual void EnterWorldPlayer(int32 GroupIndex, int64 InPlayerUid) override;
    virtual void ExitWorldPlayer(int32 GroupIndex, int64 InPlayerUid) override;
    virtual bool GetEnteredPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) override;
    virtual bool GetExitedPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) override;

    // --- EntityState 복원 큐 ---
    virtual void PushEntityStates(int32 GroupIndex, const TArray<FHktEntityState>& InStates) override;
    virtual bool GetEntityStatesToRestore(int32 GroupIndex, TArray<FHktEntityState>& OutStates) override;

    // --- Batch 조립 ---
    virtual FHktSimulationEvent& CreateOrGetGroupFrameBatch(int32 GroupIndex) override;
    virtual int32 ClaimPayloadSlots(int32 Count) override;
    virtual TArray<FHktFrameSendPayload>& GetMutablePayloads() override;
    virtual TArray<int64>& GetMutableNewbieOwners(int32 GroupIndex) override;
    virtual TArrayView<const FHktFrameSendPayload> GetValidPayloads() const override;

    // === 추가 API ===

    /** 단일 Intent 추가 (스레드 안전) */
    void PushIntent(int32 GroupIndex, const FHktEvent& InEvent);

private:
    // --- Intent 수집 (그룹별, ResetFast에서 NumGroups로 pre-sized) ---
    TArray<TArray<FHktEvent>>         GroupIntents;

    // --- 그룹 기반 데이터 (ResetFast에서 NumGroups로 pre-sized) ---
    TArray<TArray<int64>>             EnteredPlayers;
    TArray<TArray<int64>>             ExitedPlayers;
    TArray<TArray<FHktEntityState>>   PendingEntityStates;
    TArray<FHktSimulationEvent>       GroupFrameBatches;
    TArray<TArray<int64>>             GroupNewbieOwners;

    // --- 페이로드 ---
    TArray<FHktFrameSendPayload>      GlobalPayloads;
    std::atomic<int32>                PayloadWriteOffset;
};
