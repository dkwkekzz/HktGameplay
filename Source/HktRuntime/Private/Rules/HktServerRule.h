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

    virtual void SendFrameBatch(const FHktFrameBatch& Batch) = 0;
    virtual void SendInitialSimulationState(const FHktGroupSimulationState& InitialState) = 0;
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
    virtual void PushIntents(int64 InPlayerUid, const TArray<FHktIntentEvent>& InEvents) = 0;
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
    virtual FHktFrameBatch& CreateOrGetGroupFrameBatch(int32 InGroupIdx) = 0;
    virtual const FHktFrameBatch& GetGroupFrameBatch(int32 InGroupIdx) const = 0;
    virtual TArray<int64>& GetMutableNewbieOwners(int32 InGroupIdx) = 0;
    virtual const TArray<int64>& GetNewbieOwners(int32 InGroupIdx) const = 0;
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
    virtual void OnReceived_FireIntentEvent(const FHktIntentEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktIntentCollector& InCollector) override;
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) override;
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) override;
    virtual void OnEvent_RequestAutosave(int64 PlayerUid) override;
    virtual void OnTick_ProcessPendingConnections(IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldDatabase& InDB, TFunction<IHktWorldPlayer*(const FHktPlayerRecord&)> PlayerFactory) override;
    virtual void OnTick_ExecuteFrame(float InDeltaTime, const IHktFrameManager& InFrame, const IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktBatchBuilder& OutBuilder) override;
    virtual void OnTick_SendFrameBatch(const IHktRelevancyGraph& InGraph, const IHktBatchBuilder& InBuilder) override;

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
