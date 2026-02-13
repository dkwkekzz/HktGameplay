#pragma once

#include "CoreMinimal.h"
#include "HktRuntimeTypes.h"
#include "HktRuleInterfaces.h"
#include "Containers/ArrayView.h"

// Forward declarations
class AActor;

//=============================================================================
// FHktFrameSendPayload - 전송할 데이터를 묶어두는 페이로드 구조체
// 포인터만 들고 있으므로 데이터의 복사는 발생하지 않습니다.
//=============================================================================
struct FHktFrameSendPayload
{
    AActor* TargetActor = nullptr;
    int64 PlayerUid = 0;
    
    // 구조체 내부에는 포인터만 저장하여 대용량 데이터 복사 방지
    const FHktWorldState* StateToSend = nullptr;
    const FHktSimulationEvent* BatchToSend = nullptr;
};

// ============================================================================
// IHktServerSimulator - 결정론적 시뮬레이터
//
// 서버: RelevancyGroup별로 하나씩 소유 (GridRelevancyComponent 내부)
//
// 공통 흐름:
//   Execute(Batch)    : FrameBatch(입력)로 한 프레임 시뮬레이션
//   RestoreState(State): 시뮬레이션 결과로 상태 즉시 복원 (신규 유저)
//   GetSimulationState(): 현재 시뮬레이션 상태 조회
// ============================================================================
class IHktServerSimulator
{
public:
    virtual ~IHktServerSimulator() = default;

    /** FrameBatch(입력)로 한 프레임 시뮬레이션 */
    virtual void Execute(const FHktSimulationEvent& InBatch) = 0;

    /** 현재 시뮬레이션 상태 조회 (Newbie 전송, 저장 등에 사용) */
    virtual const FHktWorldState& GetSimulationState() const = 0;

    virtual FHktRuntimeOwnerState GetOwnerState(int64 InOwnerId) const = 0;
};

//=============================================================================
// IHktWorldPlayer
//=============================================================================
class IHktWorldPlayer
{
public:
    virtual ~IHktWorldPlayer() = default;
    virtual int64 GetPlayerUid() const = 0;
    virtual AActor* GetOwnerActor() const = 0;
};

//=============================================================================
// IHktIntentCollector
//=============================================================================
class IHktIntentCollector
{
public:
    virtual ~IHktIntentCollector() = default;
    virtual bool GetIntents(int64 InPlayerUid, TArray<FHktEvent>& OutIntents) = 0;
    virtual bool GetEnteredPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) = 0;
    virtual bool GetExitedPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) = 0;
    virtual void PushIntents(int64 InPlayerUid, const TArray<FHktEvent>& InEvents) = 0;
    virtual void EnterWorldPlayer(int32 GroupIndex, int64 InPlayerUid) = 0;
    virtual void ExitWorldPlayer(int32 GroupIndex, int64 InPlayerUid) = 0;
};

//=============================================================================
// IHktBatchBuilder (NewbieState 제거됨 → Simulator::GetSimulationState()로 직접 접근)
//=============================================================================
class IHktBatchBuilder
{
public:
    virtual ~IHktBatchBuilder() = default;
    
    // 프레임 시작 전 초기화
    // MaxTotalPlayers: 서버의 최대 동접자 수 (여유 있게 잡음, 예: 10000)
    virtual void ResetFast(int32 NumGroups, int32 MaxTotalPlayers) = 0;
    
    // [핵심] 쓰기 공간 확보 (Lock-Free)
    // 필요한 개수(Count)를 요청하면, 쓰기 시작할 인덱스(StartIndex)를 반환
    virtual int32 ClaimPayloadSlots(int32 Count) = 0;
    
    // 읽기 전용 접근자 (Send 단계에서 사용)
    // 전체 배열이 아니라, 실제 유효한 데이터가 있는 범위까지만 TArrayView 등으로 반환하면 더 좋음
    virtual TArrayView<const FHktFrameSendPayload> GetValidPayloads() const = 0;
    
    virtual FHktSimulationEvent& CreateOrGetGroupFrameBatch(int32 GroupIndex) = 0;
    virtual TArray<FHktFrameSendPayload>& GetMutablePayloads() = 0;
    virtual TArray<int64>& GetMutableNewbieOwners(int32 GroupIndex) = 0;
};

//=============================================================================
// IHktRelevancyGroup
//=============================================================================
class IHktRelevancyGroup
{
public:
    virtual ~IHktRelevancyGroup() = default;
    virtual IHktServerSimulator& GetSimulator() = 0;
    virtual const TArray<int64>& GetPlayerUids() const = 0;
    virtual const TArray<IHktWorldPlayer*>& GetCachedWorldPlayers() const = 0;
};

//=============================================================================
// IHktRelevancyGraph
//=============================================================================
class IHktRelevancyGraph
{
public:
    virtual ~IHktRelevancyGraph() = default;
    virtual void RegisterPlayer(IHktWorldPlayer* Player, int32 GroupIndex) = 0;
    virtual void UnregisterPlayer(int64 PlayerUid) = 0;
    virtual void UpdateRelevancy() = 0;
    virtual IHktWorldPlayer* GetWorldPlayer(int64 PlayerUid) const = 0;
    virtual int32 GetWorldPlayerCount() const = 0;
    virtual int32 GetGroupIndexByLocation(const FVector& Location) const = 0;
    virtual int32 NumRelevancyGroup() const = 0;
    virtual IHktRelevancyGroup& GetRelevancyGroup(int32 Index) = 0;
    virtual const IHktRelevancyGroup& GetRelevancyGroup(int32 Index) const = 0;
    virtual IHktRelevancyGroup* GetRelevancyGroupByPlayer(int64 PlayerUid) = 0;
    virtual const IHktRelevancyGroup* GetRelevancyGroupByPlayer(int64 PlayerUid) const = 0;
};

//=============================================================================
// IHktWorldDatabase
//=============================================================================
class IHktWorldDatabase
{
public:
    virtual ~IHktWorldDatabase() = default;
    virtual void LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(TUniquePtr<FHktPlayerRecord>)> InCallback) = 0;
    virtual void SavePlayerRecordAsync(FHktPlayerRecord InRecord) = 0;
};

//=============================================================================
// IHktFrameManager
//=============================================================================
class IHktFrameManager
{
public:
    virtual ~IHktFrameManager() = default;
    virtual bool IsInitialized() const = 0;
    virtual int64 GetFrameNumber() const = 0;
    virtual void AdvanceFrame() = 0;
};

//=============================================================================
// IHktAuthenticator / IHktPrincipal
//=============================================================================
class IHktPrincipal
{
public:
    virtual ~IHktPrincipal() = default;
    virtual const FString& GetLoginID() const = 0;
    virtual const FString& GetLoginPW() const = 0;
    virtual const FString& GetAuthenticationToken() const = 0;
    virtual bool IsAuthenticated() const = 0;
};

class IHktAuthenticator
{
public:
    virtual ~IHktAuthenticator() = default;
    virtual void Authenticate(const FString& ID, const FString& PW, TFunction<void(bool bSuccess, const FString& Token)> ResultCallback) = 0;
    virtual void Deauthenticate(const FString& Token) = 0;
};

//=============================================================================
// FHktDefaultServerRule
//=============================================================================
class HKTRUNTIME_API FHktDefaultServerRule : public IHktServerRule
{
public:
    FHktDefaultServerRule();
    virtual ~FHktDefaultServerRule();

    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) override;
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) override {}
    virtual void OnReceived_FireIntentEvent(const FHktEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector) override;
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) override;
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) override;
    virtual void OnEvent_RequestAutosave(int64 PlayerUid) override;
    virtual void OnTick_ProcessPendingConnections(IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldDatabase& InDB, TFunction<IHktWorldPlayer*(const FHktPlayerRecord&)> PlayerFactory) override;
    virtual void OnTick_ProcessSimulationAndPayloads(float InDeltaTime, const IHktFrameManager& InFrame, const IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& InOutBuilder) override;

private:
    struct FPendingLoginResult
    {
        int64 PlayerUid;
        TUniquePtr<FHktPlayerRecord> Record;
    };

    TQueue<FPendingLoginResult, EQueueMode::Mpsc> PendingLoginResults;
    TQueue<int64, EQueueMode::Mpsc> PendingLogoutRequests;
    TQueue<int64, EQueueMode::Mpsc> PendingAutosaveRequests;
    FCriticalSection AutosaveQueueLock;
    TSet<int64> QueuedAutosaveUids;
};
