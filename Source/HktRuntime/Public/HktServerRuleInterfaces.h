// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HktCoreTypes.h"
#include "HktDatabaseTypes.h"
#include "HktRuntimeTypes.h"
#include "Containers/ArrayView.h"
#include "HktServerRuleInterfaces.generated.h"

// Forward declarations
class AActor;
class IHktAuthoritySimulator;

//=============================================================================
// FHktFrameSendPayload - 전송할 데이터를 묶어두는 페이로드 구조체
// 포인터만 들고 있으므로 데이터의 복사는 발생하지 않습니다.
//=============================================================================
struct HKTRUNTIME_API FHktFrameSendPayload
{
    AActor* TargetActor = nullptr;
    int64 PlayerUid = 0;
    
    // 구조체 내부에는 포인터만 저장하여 대용량 데이터 복사 방지
    const FHktWorldState* StateToSend = nullptr;
    const FHktSimulationEvent* BatchToSend = nullptr;
};

//=============================================================================
// IHktWorldPlayer
//=============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktWorldPlayer : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktWorldPlayer
{
	GENERATED_BODY()

public:
	virtual int64 GetPlayerUid() const = 0;
	virtual AActor* GetOwnerActor() const = 0;
	
	/** 초기화 여부 확인 */
	virtual bool IsInitialized() const = 0;
	
	/** PlayerState 변경 시 캐시를 무효화합니다. */
	virtual void InvalidatePlayerUidCache() = 0;
};

//=============================================================================
// IHktSimulationEventBuilder - 그룹별 SimulationEvent 구성을 위한 통합 인터페이스
// Intent 수집(재료)과 Batch 조립(묶기)을 단일 객체로 처리합니다.
//=============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktSimulationEventBuilder : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktSimulationEventBuilder
{
	GENERATED_BODY()

public:
	// --- 프레임 초기화/정리 ---
	// MaxTotalPlayers: 서버의 최대 동접자 수 (여유 있게 잡음, 예: 10000)
	virtual void ResetFast(int32 NumGroups, int32 MaxTotalPlayers) = 0;
	virtual void EndFrame() = 0;

	// --- Intent 입력 ---
	virtual void PushIntents(int32 GroupIndex, const TArray<FHktEvent>& InEvents) = 0;
	virtual bool GetIntents(int32 GroupIndex, TArray<FHktEvent>& OutIntents) = 0;

	// --- 플레이어 진입/퇴장 ---
	virtual void EnterWorldPlayer(int32 GroupIndex, int64 InPlayerUid) = 0;
	virtual void ExitWorldPlayer(int32 GroupIndex, int64 InPlayerUid) = 0;
	virtual bool GetEnteredPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) = 0;
	virtual bool GetExitedPlayers(int32 GroupIndex, TArray<int64>& OutPlayerUids) = 0;

	// --- EntityState 복원 큐 ---
	virtual void PushEntityStates(int32 GroupIndex, const TArray<FHktEntityState>& InStates) = 0;
	virtual bool GetEntityStatesToRestore(int32 GroupIndex, TArray<FHktEntityState>& OutStates) = 0;

	// --- Batch 조립 (Lock-Free Payload) ---
	virtual FHktSimulationEvent& CreateOrGetGroupFrameBatch(int32 GroupIndex) = 0;
	virtual int32 ClaimPayloadSlots(int32 Count) = 0;
	virtual TArray<FHktFrameSendPayload>& GetMutablePayloads() = 0;
	virtual TArray<int64>& GetMutableNewbieOwners(int32 GroupIndex) = 0;
	virtual TArrayView<const FHktFrameSendPayload> GetValidPayloads() const = 0;
};

//=============================================================================
// IHktRelevancyGroup
//=============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktRelevancyGroup : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktRelevancyGroup
{
	GENERATED_BODY()

public:
	virtual IHktAuthoritySimulator& GetSimulator() = 0;
	virtual const TArray<int64>& GetPlayerUids() const = 0;
	virtual const TArray<IHktWorldPlayer*>& GetCachedWorldPlayers() const = 0;
};

//=============================================================================
// IHktRelevancyGraph
//=============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktRelevancyGraph : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktRelevancyGraph
{
	GENERATED_BODY()

public:
	virtual void RegisterPlayer(IHktWorldPlayer* Player, int32 GroupIndex) = 0;
	virtual void UnregisterPlayer(int64 PlayerUid) = 0;
	virtual void UpdateRelevancy() = 0;
	virtual IHktWorldPlayer* GetWorldPlayer(int64 PlayerUid) const = 0;
	virtual int32 GetWorldPlayerCount() const = 0;
	virtual int32 NumRelevancyGroup() const = 0;
	virtual IHktRelevancyGroup& GetRelevancyGroup(int32 Index) = 0;
	virtual const IHktRelevancyGroup& GetRelevancyGroup(int32 Index) const = 0;
	virtual IHktRelevancyGroup* GetRelevancyGroupByPlayer(int64 PlayerUid) = 0;
	virtual const IHktRelevancyGroup* GetRelevancyGroupByPlayer(int64 PlayerUid) const = 0;
	
	/** 플레이어가 속한 그룹의 인덱스를 반환합니다. 플레이어를 찾을 수 없으면 INDEX_NONE을 반환합니다. */
	virtual int32 GetRelevancyGroupIndex(int64 PlayerUid) const = 0;
};

//=============================================================================
// IHktWorldDatabase
//=============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktWorldDatabase : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktWorldDatabase
{
	GENERATED_BODY()

public:
	virtual void LoadPlayerRecordAsync(int64 InPlayerUid, TFunction<void(TUniquePtr<FHktPlayerRecord>)> InCallback) = 0;
	virtual void SavePlayerRecordAsync(int64 InPlayerUid, FHktPlayerState&& InState) = 0;
};

//=============================================================================
// IHktFrameManager
//=============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktFrameManager : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktFrameManager
{
	GENERATED_BODY()

public:
	virtual bool IsInitialized() const = 0;
	virtual int64 GetFrameNumber() const = 0;
	virtual void AdvanceFrame() = 0;
};

//=============================================================================
// IHktAuthenticator / IHktPrincipal
//=============================================================================

UINTERFACE(MinimalAPI, BlueprintType)
class UHktPrincipal : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktPrincipal
{
	GENERATED_BODY()

public:
	virtual const FString& GetLoginID() const = 0;
	virtual const FString& GetLoginPW() const = 0;
	virtual const FString& GetAuthenticationToken() const = 0;
	virtual bool IsAuthenticated() const = 0;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UHktAuthenticator : public UInterface
{
	GENERATED_BODY()
};

class HKTRUNTIME_API IHktAuthenticator
{
	GENERATED_BODY()

public:
	virtual void Authenticate(const FString& ID, const FString& PW, TFunction<void(bool bSuccess, const FString& Token)> ResultCallback) = 0;
	virtual void Deauthenticate(const FString& Token) = 0;
};

//=============================================================================
// IHktServerRule
//=============================================================================

class HKTRUNTIME_API IHktServerRule
{
public:
    virtual ~IHktServerRule() = default;

    // --- 인증 ---
    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) {}
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) {}

    // --- Intent 수신 ---
    virtual void OnReceived_FireIntentEvent(const FHktEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktRelevancyGraph& InGraph, IHktSimulationEventBuilder& InBuilder) {}

    // --- 로그인/로그아웃 ---
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) {}
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) {}

    // --- 틱 ---
    virtual void OnTick_ProcessReady(IHktFrameManager& InFrame) {}
    virtual void OnTick_ProcessPendingConnections(IHktRelevancyGraph& InGraph, IHktSimulationEventBuilder& InBuilder, IHktWorldDatabase& InDB) {}
    virtual void OnTick_ProcessSimulationAndPayloads(float InDeltaTime, const IHktFrameManager& InFrame, const IHktRelevancyGraph& InGraph, IHktSimulationEventBuilder& InOutBuilder) {}
};
