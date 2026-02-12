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
class IHktServerSimulator;
class IHktClientSimulator;
class IHktUnitSelectionPolicy;
class IHktCommandContainer;
class IHktIntentBuilder;
class UWorld;
struct FHktFrameSendPayload;

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
    virtual void OnReceived_FireIntentEvent(const FHktEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector) {}

    // --- 로그인/로그아웃 ---
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) {}
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) {}

    // --- 틱 ---
    virtual void OnEvent_RequestAutosave(int64 PlayerUid) {}
    virtual void OnTick_ProcessPendingConnections(IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldDatabase& InDB, TFunction<IHktWorldPlayer*(const FHktPlayerRecord&)> PlayerFactory) {}
    virtual void OnTick_ExecuteFrame(float InDeltaTime, const IHktFrameManager& InFrame, const IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder) {}
    virtual void OnTick_PrepareSendPayloads(const IHktRelevancyGraph& InGraph, const IHktBatchBuilder& InBuilder, TArray<FHktFrameSendPayload>& OutPayloads) {}
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
    virtual void OnUserEvent_SubjectInputAction(const IHktUnitSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_TargetInputAction(const IHktUnitSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) = 0;

    // === 서버 수신 ===

    /** 신규 유저: 그룹 시뮬레이션 결과를 받아 즉시 동기화 (접속 직후 1회) */
    virtual void OnReceived_InitialSimulationState(const FHktWorldState& InState, IHktClientSimulator& InSimulator) = 0;

    /** 기존 유저: FrameBatch(입력)를 받아 로컬 시뮬레이션 수행 (매 프레임) */
    virtual void OnReceived_FrameBatch(const FHktSimulationEvent& InBatch, IHktClientSimulator& InSimulator) = 0;
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
