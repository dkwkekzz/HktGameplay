#pragma once

#include "CoreMinimal.h"
#include "HktRuntimeTypes.h"
#include "HktServerRuleInterfaces.h"

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
    virtual void OnReceived_FireIntentEvent(const FHktEvent& InEvent, const IHktWorldPlayer& InPlayer, IHktRelevancyGraph& InGraph, IHktSimulationEventBuilder& InBuilder) override;
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) override;
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB) override;
    virtual void OnTick_ProcessReady(IHktFrameManager& InFrame) override;
    virtual void OnTick_ProcessPendingConnections(IHktRelevancyGraph& InGraph, IHktSimulationEventBuilder& InBuilder, IHktWorldDatabase& InDB) override;
    virtual void OnTick_ProcessSimulationAndPayloads(float InDeltaTime, const IHktFrameManager& InFrame, const IHktRelevancyGraph& InGraph, IHktSimulationEventBuilder& InOutBuilder, const IHktWorldDatabase& InDB) override;

private:
    struct FPendingLoginResult
    {
        TWeakInterfacePtr<IHktWorldPlayer> WeakPlayer;
        const FHktPlayerRecord* Record;
    };

    TQueue<FPendingLoginResult, EQueueMode::Mpsc> PendingLoginResults;
    TQueue<int64, EQueueMode::Mpsc> PendingLogoutRequests;
};
