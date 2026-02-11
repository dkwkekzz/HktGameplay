// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktDatabaseTypes.h"
#include "HktRuntimeTypes.h"

// === Forward Declarations ===
class IHktAuthenticator;
class IHktBatchBuilder;
class IHktIntentCollector;
class IHktPrincipal;
class IHktWorldPlayer;
class IHktRelevancyGraph;
class IHktWorldDatabase;
class IHktFrameManager;
class IHktSimulator;
class IHktSubjectSelectionPolicy;
class IHktTargetSelectionPolicy;
class IHktCommandContainer;
class IHktIntentBuilder;
class UWorld;

// ============================================================================
// IHktSimulator - 결정론적 시뮬레이터 (서버/클라이언트 공용)
//
// 서버: RelevancyGroup별로 하나씩 소유 (GridRelevancyComponent 내부)
// 클라이언트: PlayerController가 하나 소유 (ClientSimulatorComponent)
//
// 공통 흐름:
//   Execute(Batch)    : FrameBatch(입력)로 한 프레임 시뮬레이션
//   RestoreState(State): 시뮬레이션 결과로 상태 즉시 복원 (신규 유저)
//   GetSimulationState(): 현재 시뮬레이션 상태 조회
// ============================================================================
class IHktSimulator
{
public:
    virtual ~IHktSimulator() = default;

    /** FrameBatch(입력)로 한 프레임 시뮬레이션 */
    virtual void Execute(const FHktRuntimeBatch& InBatch) = 0;

    /** 시뮬레이션 결과로 상태 즉시 복원 + 대기 중이던 배치 재생 */
    virtual void RestoreState(const FHktRuntimeSimulationState& InState, TArray<FHktRuntimeBatch>&& InPendingBatches) = 0;

    /** 현재 시뮬레이션 상태 조회 (Newbie 전송, 저장 등에 사용) */
    virtual const FHktRuntimeSimulationState& GetSimulationState() const = 0;

    virtual FHktRuntimeOwnerState GetOwnerState(int64 InOwnerId) const = 0;

    /** 초기화 완료 여부 (RestoreState 또는 첫 Execute 이후 true) */
    virtual bool IsInitialized() const = 0;
};

// ============================================================================
// IHktServerRule
// ============================================================================
class IHktServerRule
{
public:
    virtual ~IHktServerRule() = default;

    // --- 인증 ---
    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) {}
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) {}

    // --- Intent 수신 ---
    virtual void OnReceived_FireIntentEvent(const FHktRuntimeEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector) {}

    // --- 로그인/로그아웃 ---
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) {}
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) {}

    // --- 틱 ---
    virtual void OnEvent_RequestAutosave(int64 PlayerUid) {}
    virtual void OnTick_ProcessPendingConnections(IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldDatabase& InDB, TFunction<IHktWorldPlayer*(const FHktPlayerRecord&)> PlayerFactory) {}
    virtual void OnTick_ExecuteFrame(float InDeltaTime, const IHktFrameManager& InFrame, const IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder) {}
    virtual void OnTick_SendFrameBatch(const IHktRelevancyGraph& InGraph, const IHktBatchBuilder& InBuilder) {}
};

// ============================================================================
// IHktClientRule
// ============================================================================
class IHktClientRule
{
public:
    virtual ~IHktClientRule() = default;

    // === 유저 입력 ===
    virtual void OnUserEvent_LoginButtonClick() = 0;
    virtual void OnUserEvent_SubjectInputAction(const IHktSubjectSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_TargetInputAction(const IHktTargetSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) = 0;

    // === 서버 수신 ===

    /** 신규 유저: 그룹 시뮬레이션 결과를 받아 즉시 동기화 (접속 직후 1회) */
    virtual void OnReceived_InitialSimulationState(const FHktRuntimeSimulationState& InState, IHktSimulator& InSimulator) = 0;

    /** 기존 유저: FrameBatch(입력)를 받아 로컬 시뮬레이션 수행 (매 프레임) */
    virtual void OnReceived_FrameBatch(const FHktRuntimeBatch& InBatch, IHktSimulator& InSimulator) = 0;
};

// ============================================================================
// 팩토리 함수
// ============================================================================

namespace HktRule
{
    /** IHktServerRule 인스턴스 생성 (HktRuntime 내부 구현) */
    HKTRUNTIME_API TSharedPtr<IHktServerRule> GetServerRule(UWorld* InWorld);
    
    /** IHktClientRule 인스턴스 생성 (HktRuntime 내부 구현) */
    HKTRUNTIME_API TSharedPtr<IHktClientRule> GetClientRule(UWorld* InWorld);
}
