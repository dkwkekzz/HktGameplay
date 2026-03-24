#pragma once

#include "CoreMinimal.h"
#include "HktServerRuleInterfaces.h"
#include "HktBagTypes.h"

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
        IHktWorldDatabase* InDB) override;

    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal, TFunction<void(bool bSuccess, const FString& Token)> InResultCallback) override;
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) override {}
    virtual void OnReceived_SlotRequest(const FHktSlotRequest& InRequest, const IHktWorldPlayer& InPlayer) override;
    virtual void OnReceived_MoveRequest(const FHktMoveRequest& InRequest, const IHktWorldPlayer& InPlayer) override;
    virtual void OnReceived_ItemRequest(const FHktItemRequest& InRequest, const IHktWorldPlayer& InPlayer) override;
    virtual void OnReceived_BagRequest(const FHktBagRequest& InRequest, const IHktWorldPlayer& InPlayer) override;

    // 액터 이벤트 (item 1)
    virtual void OnEvent_GameModePostLogin(const IHktWorldPlayer& InPlayer) override;
    virtual void OnEvent_GameModeLogout(const IHktWorldPlayer& InPlayer) override;
    virtual FHktEventGameModeTickResult OnEvent_GameModeTick(float InDeltaTime) override;

private:
    // 바인딩된 컨텍스트 (item 2)
    IHktFrameManager*             CachedFrame   = nullptr;
    IHktRelevancyGraph*           CachedGraph   = nullptr;
    IHktWorldDatabase*            CachedDB      = nullptr;

    struct FPendingLoginResult
    {
        TWeakInterfacePtr<IHktWorldPlayer> WeakPlayer;
        FHktPlayerRecord Record;
    };

    TQueue<FPendingLoginResult, EQueueMode::Mpsc> PendingLoginResults;
    TQueue<int64, EQueueMode::Mpsc>               PendingLogoutRequests;
	TArray<TArray<FHktEvent>>                     PendingGroupIntents;

	int32 ServerEventSequence = 0;

	// NPC 스포너 — fire된 스포너 EventTag 추적 (그룹별)
	TSet<FGameplayTag> ActiveSpawnerFlows;

	// 가방 요청 큐 — SimulationTick에서 BagComponent와 함께 처리
	struct FPendingBagRequest
	{
		FHktBagRequest Request;
		int64 PlayerUid = 0;
		int32 GroupIndex = INDEX_NONE;
	};
	TArray<FPendingBagRequest> PendingBagRequests;
};
