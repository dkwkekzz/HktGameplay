#pragma once

#include "CoreMinimal.h"
#include "HktServerRuleInterfaces.h"

struct FHktPlayerRecord;

//=============================================================================
// FHktDefaultServerRule
//=============================================================================
class HKTRULE_API FHktDefaultServerRule : public IHktServerRule
{
public:
    FHktDefaultServerRule();
    virtual ~FHktDefaultServerRule();

    // 컨텍스트 바인딩 (item 2)
    virtual void BindContext(
        IHktFrameManager* InFrame,
        IHktRelevancyGraph* InGraph,
        IHktWorldDatabase* InDB,
        IHktSimulationEventBuilder* InBuilder) override;

    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) override;
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) override {}
    virtual void OnReceived_FireIntentEvent(const FHktEvent& InEvent, const IHktWorldPlayer& InPlayer) override;

    // 액터 이벤트 (item 1)
    virtual void OnEvent_GameModePostLogin(const IHktWorldPlayer& InPlayer) override;
    virtual void OnEvent_GameModeLogout(const IHktWorldPlayer& InPlayer) override;
    virtual FHktEventGameModeTickResult OnEvent_GameModeTick(float InDeltaTime) override;

private:
    // 바인딩된 컨텍스트 (item 2)
    IHktFrameManager*             CachedFrame   = nullptr;
    IHktRelevancyGraph*           CachedGraph   = nullptr;
    IHktWorldDatabase*            CachedDB      = nullptr;
    IHktSimulationEventBuilder*   CachedBuilder = nullptr;

    struct FPendingLoginResult
    {
        TWeakInterfacePtr<IHktWorldPlayer> WeakPlayer;
        const FHktPlayerRecord* Record;
    };

    TQueue<FPendingLoginResult, EQueueMode::Mpsc> PendingLoginResults;
    TQueue<int64, EQueueMode::Mpsc>               PendingLogoutRequests;
};
