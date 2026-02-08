#pragma once

#include "CoreMinimal.h"
#include "HktRuntimeTypes.h"

class IHktAuthenticator;
class IHktIntentCollector;
class IHktPrincipal;
class IHktWorldPlayer;
class IHktRelevancyGraph;
class IHktWorldDatabase;
class IHktPersistentFrame;
class IHktSimulator;
class IHktWorldState;

class IHktServerRule
{
public:
    virtual ~IHktServerRule() = default;
    virtual void OnReceived_Authentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) {}
    virtual void OnReceived_Deauthentication(IHktAuthenticator& Authenticator, const IHktPrincipal& InPrincipal) {}
    virtual void OnReceived_FireIntentEvent(IHktIntentCollector& InCollector, const FHktIntentEvent& InEvent) {}
    virtual void OnLogin_EnterWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB, IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldState& InState) {}
    virtual void OnLogout_ExitWorldPlayer(const IHktWorldPlayer& InPlayer, IHktWorldDatabase& InDB, IHktRelevancyGraph& InGraph, IHktIntentCollector& InCollector, IHktWorldState& InState) {}
    virtual void OnTick_UpdateFrame(IHktPersistentFrame& InFrame, IHktRelevancyGraph& InGraph, IHktWorldState& InState) {}
    virtual void OnTick_SendFrameBatch(const IHktRelevancyGraph& InGraph, IHktWorldState& InState, IHktWorldPlayer& InPlayer) {}
    virtual void OnTick_ExecuteFrame(const IHktPersistentFrame& InFrame, const IHktRelevancyGraph& InGraph, IHktSimulator& InSimulator) {}
};

class IHktSubjectSelectionPolicy;
class IHktTargetSelectionPolicy;
class IHktCommandContainer;
class IHktIntentBuilder;
struct FHktFrameBatch;

class IHktClientRule
{
public:
    virtual ~IHktClientRule() = default;
    virtual void OnUserEvent_LoginButtonClick() = 0;
    virtual void OnUserEvent_SubjectInputAction(const IHktSubjectSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_TargetInputAction(const IHktTargetSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) = 0;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) = 0;
    virtual void OnReceived_FrameBatch(const FHktFrameBatch& InBatch, IHktWorldState& InState, IHktSimulator& InSimulator) = 0;
};
