// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Rules/HktServerRule.h"
#include "HktIntentCollectorComponent.generated.h"

/**
 * UHktIntentCollectorComponent - IHktIntentCollector 구현
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - Actor(GameMode)는 이 컴포넌트를 Rule에 인터페이스로 전달
 *
 * 역할:
 *   - 플레이어별 Intent 큐 관리 (PushIntent → GetIntents)
 *   - 플레이어 진입/퇴장 큐 관리 (EnterWorldPlayer / ExitWorldPlayer)
 *   - 프레임 종료 시 소비된 데이터 정리 (EndFrame)
 *
 * 스레드 안전성:
 *   - PushIntent는 여러 스레드에서 호출 가능 (Lock 보호)
 *   - GetIntents, EndFrame은 메인 스레드에서만 호출
 */
UCLASS(ClassGroup=(HktSimulation), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktIntentCollectorComponent : public UActorComponent, public IHktIntentCollector
{
    GENERATED_BODY()

public:
    UHktIntentCollectorComponent();

    // === IHktIntentCollector 구현 ===

    virtual bool GetIntents(int64 InPlayerUid, TArray<FHktRuntimeEvent>& OutIntents) override;
    virtual bool GetEnteredPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) override;
    virtual bool GetExitedPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) override;
    virtual void PushIntents(int64 InPlayerUid, const TArray<FHktRuntimeEvent>& InEvents) override;
    virtual void EnterWorldPlayer(int32 GroupIndex, int64 InPlayerUid) override;
    virtual void ExitWorldPlayer(int32 GroupIndex, int64 InPlayerUid) override;

    // === 추가 API (Rule 시그니처의 PushIntent 단일 이벤트) ===

    /** 단일 Intent 추가 (스레드 안전) */
    void PushIntent(int64 InPlayerUid, const FHktRuntimeEvent& InEvent);

    /** 프레임 종료 시 소비된 데이터 정리 */
    void EndFrame();

private:
    // 플레이어별 Intent 큐
    FCriticalSection IntentLock;
    TMap<int64, TArray<FHktRuntimeEvent>> PlayerIntents;

    // 그룹별 진입/퇴장 큐
    TMap<int32, TArray<int64>> EnteredPlayers;
    TMap<int32, TArray<int64>> ExitedPlayers;
};
