#pragma once

#include "CoreMinimal.h"
#include "HktRuntimeTypes.h"
#include "HktRuleInterfaces.h"

//=============================================================================
// IHktWorldPlayer
//=============================================================================
class IHktWorldPlayer
{
public:
    virtual ~IHktWorldPlayer() = default;
    
    virtual int64 GetPlayerUid() const = 0;

    // Existing Users: Batch(Input)를 받아 시뮬레이션 수행
    virtual void Client_ReceiveFrameBatch(const FHktFrameBatch& Batch) = 0;

    // Newbie Users: State(Result)를 받아 즉시 동기화 (Batch 처리 생략)
    virtual void Client_ReceiveInitialState(const FHktGroupSimulationState& InitialState) = 0;
};

//=============================================================================
// IHktIntentCollector
//=============================================================================
class IHktIntentCollector
{
public:
    virtual ~IHktIntentCollector() = default;
    
    virtual bool GetIntents(int64 InPlayerUid, TArray<FHktIntentEvent>& OutIntents) = 0;
    virtual bool GetEnteredPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) = 0;
    virtual bool GetExitedPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) = 0;

    // [추가] 로그인 시 초기 스폰/복구 이벤트를 주입하기 위함
    virtual void PushIntents(int64 InPlayerUid, const TArray<FHktIntentEvent>& InEvents) = 0;
    
    // [추가] Relevancy 변경 알림 (BatchBuilder가 Newbie 목록을 만들 때 사용)
    virtual void EnterWorldPlayer(int32 GroupIndex, int64 InPlayerUid) = 0;
    virtual void ExitWorldPlayer(int32 GroupIndex, int64 InPlayerUid) = 0;
};

//=============================================================================
// IHktBatchBuilder
//=============================================================================
class IHktBatchBuilder
{
public:
    virtual ~IHktBatchBuilder() = default;

    virtual FHktFrameBatch& CreateOrGetGroupFrameBatch(int32 InGroupIdx) = 0;
    virtual TArray<int64>& GetMutableNewbieOwners(int32 InGroupIdx) = 0;
    virtual FHktGroupSimulationState& CreateOrGetNewbieState(int32 InGroupIdx) = 0;

    virtual const FHktFrameBatch& GetGroupFrameBatch(int32 InGroupIdx) const = 0;
    virtual const TArray<int64>& GetNewbieOwners(int32 InGroupIdx) const = 0;
    virtual const FHktGroupSimulationState* GetNewbieState(int32 InGroupIdx) const = 0;
};

//=============================================================================
// IHktRelevancyGroup
//=============================================================================
class IHktRelevancyGroup
{
public:
    virtual ~IHktRelevancyGroup() = default;
    virtual IHktSimulator& GetSimulator() = 0;
    virtual const TArray<int64>& GetPlayerUids() const = 0;
    virtual const TArray<IHktWorldPlayer*>& GetCachedWorldPlayers() const = 0;
    virtual const FHktGroupSimulationState& GetCurrentSimulationState() const = 0;
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
    
    // [추가] 위치 기반으로 그룹 인덱스를 찾는 헬퍼 함수
    virtual int32 GetGroupIndexByLocation(const FVector& Location) const = 0;

    virtual int32 NumRelevancyGroup() const = 0;
    virtual IHktRelevancyGroup& GetRelevancyGroup(int32 Index) = 0;
    virtual const IHktRelevancyGroup& GetRelevancyGroup(int32 Index) const = 0;
};

//=============================================================================
// IHktWorldDatabase - 플레이어/엔티티 DB
//=============================================================================
class IHktWorldDatabase
{
public:
    virtual ~IHktWorldDatabase() = default;
    
    // 비동기 로드: 완료 시 Callback 호출 (Worker Thread에서 호출될 수 있음)
    // [최적화] Load 완료 시 TUniquePtr로 소유권을 넘겨줌 (복사 방지)
    virtual void LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(TUniquePtr<FHktPlayerRecord>)> InCallback) = 0;

    // 비동기 저장: 완료 여부는 중요하지 않으나 순서는 보장되어야 함
    // [최적화] Save 시 값으로 받되, 호출자가 MoveTemp로 넘기도록 유도 (내부 TArray 포인터만 이동됨)
    virtual void SavePlayerRecordAsync(FHktPlayerRecord InRecord) = 0;
};

//=============================================================================
// IHktPersistentFrame - 프레임 번호 + 프레임 Intent + 이벤트 셀 캐시
//=============================================================================
class IHktPersistentFrame
{
public:
    virtual ~IHktPersistentFrame() = default;
    virtual bool IsInitialized() const = 0;
    virtual uint64 GetFrameNumber() const = 0;
    virtual void AdvanceFrame() = 0;
};

//=============================================================================
// IHktAuthenticator / IHktPrincipal - 인증 (기본 Rule에서는 no-op)
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

class FHktDefaultServerRule : public IHktServerRule
{
public:
	FHktDefaultServerRule();
	virtual ~FHktDefaultServerRule();

    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) override;
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) override {}
    virtual void OnReceived_FireIntentEvent(const FHktIntentEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector) override;
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) override;
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) override;
    virtual void OnTick_ExecuteFrame(const IHktPersistentFrame& InFrame, const IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder) override;
    virtual void OnTick_SendFrameBatch(const IHktRelevancyGraph& InGraph, const IHktBatchBuilder& InBuilder) override;

private:
    // [최적화] 무거운 구조체 대신 UniquePtr를 사용하여 큐 복사 비용 최소화 (8바이트 복사)
    TQueue<TUniquePtr<FHktPlayerRecord>, EQueueMode::Mpsc> PendingLoginResults;

    // 로그아웃 요청 큐 (Uid)
    TQueue<int64, EQueueMode::Mpsc> PendingLogoutRequests;

    // 수시 저장(Autosave) 요청 큐
    TQueue<int64, EQueueMode::Mpsc> PendingAutosaveRequests;
};
