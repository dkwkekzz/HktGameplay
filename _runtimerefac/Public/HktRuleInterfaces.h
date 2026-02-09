#pragma once

#include "CoreMinimal.h"
#include "HktDatabaseTypes.h"
#include "HktRuntimeTypes.h"

class IHktAuthenticator;
class IHktBatchBuilder;
class IHktIntentCollector;
class IHktPrincipal;
class IHktWorldPlayer;
class IHktRelevancyGraph;
class IHktWorldDatabase;
class IHktPersistentFrame;

// ============================================================================
// IHktSimulator - 시뮬레이터
// ============================================================================
class IHktSimulator
{
public:
    virtual ~IHktSimulator() = default;

    /** 기존 유저: FrameBatch(입력)로 한 프레임 시뮬레이션 */
    virtual void Execute(const FHktFrameBatch& InBatch) = 0;

    /** 신규 유저: 서버 시뮬레이션 결과로 상태 즉시 복원 */
    virtual void RestoreState(const FHktGroupSimulationState& InState, TArray<FHktIntentEvent>&& InPendingBatches) = 0;

    /** 시뮬레이션 상태 조회 */
    virtual const FHktGroupSimulationState& GetSimulationState() const = 0;
    
    /** 초기화 완료 여부 (RestoreState 또는 첫 Execute 이후 true) */
    virtual bool IsInitialized() const = 0;
};

class IHktServerRule
{
public:
    virtual ~IHktServerRule() = default;
    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) {}
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) {}
    virtual void OnReceived_FireIntentEvent(const FHktIntentEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector) {}
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) {}
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) {}
    virtual void OnEvent_RequestAutosave(int64 PlayerUid) {}
    virtual void OnTick_ProcessPendingConnections(IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldDatabase& InDB, TFunction<IHktWorldPlayer*(const FHktPlayerRecord&)> PlayerFactory) {}
    virtual void OnTick_ExecuteFrame(const IHktPersistentFrame& InFrame, const IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder) {}
    virtual void OnTick_SendFrameBatch(const IHktRelevancyGraph& InGraph, const IHktBatchBuilder& InBuilder) {}
};

class IHktSubjectSelectionPolicy;
class IHktTargetSelectionPolicy;
class IHktCommandContainer;
class IHktIntentBuilder;

class IHktClientRule
{
public:
    virtual ~IHktClientRule() = default;
    virtual void OnUserEvent_LoginButtonClick() = 0;
    virtual void OnUserEvent_SubjectInputAction(const IHktSubjectSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_TargetInputAction(const IHktTargetSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) = 0;
    /** 신규 유저: 그룹 시뮬레이션 결과를 받아 즉시 동기화 (접속 직후 1회) */
    virtual void OnReceived_InitialSimulationState(const FHktGroupSimulationState& InState, IHktSimulator& InSimulator) = 0;
    /** 기존 유저: FrameBatch(입력)를 받아 로컬 시뮬레이션 수행 (매 프레임) */
    virtual void OnReceived_FrameBatch(const FHktFrameBatch& InBatch, IHktSimulator& InSimulator) = 0;
};
