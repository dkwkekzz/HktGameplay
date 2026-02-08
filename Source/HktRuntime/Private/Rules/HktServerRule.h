#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktRuntimeTypes.h"
#include "HktRuleInterfaces.h"

//=============================================================================
// IHktWorldPlayer - 월드 플레이어 추상화 (Actor는 구현하지 않고 컴포넌트/어댑터가 구현)
//=============================================================================
class IHktWorldPlayer
{
public:
    virtual ~IHktWorldPlayer() = default;
    virtual int64 GetPlayerUid() const = 0;
	virtual const FString& GetPlayerName() const = 0;
    virtual FVector GetLocation() const = 0;

	virtual bool ShouldSavePlayerRecord() const = 0;
	virtual FHktPlayerRecord MakePlayerRecord() const = 0;

    virtual void SendFrameBatch(const FHktFrameBatch& Batch) = 0;
};

//=============================================================================
// IHktIntentCollector - Intent 수집 (PushIntent / ConsumeIntents)
//=============================================================================
class IHktIntentCollector
{
public:
    virtual ~IHktIntentCollector() = default;
    virtual void PushIntent(int64 InPlayerUid, const FHktIntentEvent& Event) = 0;
    virtual bool PushIntents(int64 InPlayerUid, TArray<FHktIntentEvent>& OutIntents) = 0;
    virtual bool GetIntents(int64 InPlayerUid, TArray<FHktIntentEvent>& OutIntents) = 0;

	virtual void PushEntitySnapshots(int64 InPlayerUid, const TArray<FHktEntitySnapshot>& Snapshots) = 0;
	virtual bool GetEntitySnapshots(int64 InPlayerUid, TArray<FHktEntitySnapshot>& OutSnapshots) = 0;

	virtual void EnterWorldPlayer(int64 InPlayerUid) = 0;
    virtual bool GetEnteredPlayers(TArray<int64>& OutPlayerUids) = 0;
	virtual void ExitWorldPlayer(int64 InPlayerUid) = 0;
    virtual bool GetExitedPlayers(TArray<int64>& OutPlayerUids) = 0;
};

//=============================================================================
// IHktSimulator - VMProcessor 등 시뮬레이션 실행
//=============================================================================
class IHktSimulator
{
public:
    virtual ~IHktSimulator() = default;
    virtual void Execute(const FHktFrameBatch& InBatch) = 0;
};

//=============================================================================
// IHktRelevancyGraph - Relevancy 그래프 (셀 단위: 관심 셀 목록, 셀별 플레이어)
//=============================================================================
class IHktRelevancyGroup
{
public:
    virtual ~IHktRelevancyGroup() = default;
	virtual IHktSimulator& GetSimulator() = 0;
    virtual const TArray<IHktWorldPlayer*>& GetPlayers() const = 0;
};

class IHktRelevancyGraph
{
public:
    virtual ~IHktRelevancyGraph() = default;
    virtual void RegisterPlayer(const IHktWorldPlayer& Player) = 0;
    virtual void UnregisterPlayer(const IHktWorldPlayer& Player) = 0;
    virtual void UpdateRelevancy() = 0;

	virtual int32 GetNumRelevancyGroups() const = 0;
    virtual IHktRelevancyGroup& GetRelevancyGroup(int32 Index) = 0;
    virtual const IHktRelevancyGroup& GetRelevancyGroup(int32 Index) const = 0;
};

//=============================================================================
// IHktWorldMasterState - 월드 상태 (위치, 스냅샷, Stash)
// 하나의 셀에 플레이어·엔터티·이벤트가 들어있고, 셀 단위로 FrameBatch를 구성한다.
//=============================================================================
class IHktWorldMasterState
{
public:
    virtual ~IHktWorldMasterState() = default;

    virtual TArray<FHktEntityId> GetEntitiesByOwner(int64 InOwnerUid) const = 0;
    virtual TArray<FHktEntitySnapshot> GetEntitySnapshotsByOwner(int64 InOwnerUid) const = 0;
    virtual FHktEntitySnapshot CreateEntitySnapshot(FHktEntityId Entity) const = 0;
};

//=============================================================================
// IHktWorldDatabase - 플레이어/엔티티 DB
//=============================================================================
class IHktWorldDatabase
{
public:
    virtual ~IHktWorldDatabase() = default;
    virtual void GetOrCreatePlayerRecord(int64 InPlayerUid, TFunction<void(FHktPlayerRecord&)> InCallback) = 0;
    virtual const FHktPlayerRecord* GetPlayerRecord(int64 InPlayerUid) const = 0;
    virtual void SavePlayerRecord(const FHktPlayerRecord& InRecord) = 0;
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

//=============================================================================
// IHktBatchBuilder - 배치 빌더
//=============================================================================
class IHktBatchBuilder
{
public:
    virtual ~IHktBatchBuilder() = default;
	
};

class FHktDefaultServerRule : public IHktServerRule
{
public:
	FHktDefaultServerRule();
	~FHktDefaultServerRule();

    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) override;
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) override {}
    virtual void OnReceived_FireIntentEvent(const FHktIntentEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector) override;
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB, IHktIntentCollector& InCollector) override;
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB, IHktIntentCollector& InCollector) override;
    virtual void OnTick_ExecuteFrame(IHktPersistentFrame& InFrame, IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder) override;
    virtual void OnTick_SendFrameBatch(const IHktRelevancyGraph& InGraph, const IHktBatchBuilder& InBuilder) override;
};
