// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIngamePlayerController.h"
#include "HktPlayerState.h"
#include "HktClientRuleInterfaces.h"
#include "HktGameMode.h"
#include "HktRuntimeConverter.h"
#include "HktRuntimeTypes.h"
#include "HktCoreDataCollector.h"
#include "DataAssets/HktInputAction.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktIngamePlayerController, Log, All);

AHktIngamePlayerController::AHktIngamePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void AHktIngamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    // ClientRule — Standalone / Client 에서만 (DedicatedServer 제외)
    const ENetMode NetMode = GetWorld()->GetNetMode();
    if (NetMode == NM_Standalone || NetMode == NM_Client)
    {
        CachedClientRule = HktRule::GetClientRule(GetWorld());
    }

    // 컴포넌트에서 인터페이스 캐싱
    TArray<UActorComponent*> Components;
    GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (IHktIntentBuilder* IntentBuilder = Cast<IHktIntentBuilder>(Comp))
        {
            CachedIntentBuilder = IntentBuilder;
        }
        else if (IHktUnitSelectionPolicy* SelectionPolicy = Cast<IHktUnitSelectionPolicy>(Comp))
        {
            CachedSelectionPolicy = SelectionPolicy;
        }
        else if (IHktProxySimulator* ProxySimulator = Cast<IHktProxySimulator>(Comp))
        {
            CachedProxySimulator = ProxySimulator;
        }
        else if (IHktCommandContainer* CommandContainer = Cast<IHktCommandContainer>(Comp))
        {
            CachedCommandContainer = CommandContainer;
            TArray<TObjectPtr<UObject>> AsObjects;
            for (const TObjectPtr<UHktInputAction>& A : SlotActions)
            {
                AsObjects.Add(A.Get());
            }
            CommandContainer->SetSlotActions(AsObjects);
        }
        else if (IHktWorldPlayer* WorldPlayer = Cast<IHktWorldPlayer>(Comp))
        {
            CachedWorldPlayer = WorldPlayer;
        }
    }

    // 컨텍스트 바인딩 — ServerRule::BindContext와 동일한 패턴
    if (CachedClientRule)
    {
        CachedClientRule->BindContext(
            CachedProxySimulator,
            CachedIntentBuilder,
            CachedSelectionPolicy,
            CachedCommandContainer);
    }

    // ProxySimulatorComponent가 먼저 틱한 후 PlayerController Tick이 실행되도록 보장
    if (UActorComponent* ProxyComp = Cast<UActorComponent>(CachedProxySimulator))
    {
        AddTickPrerequisiteComponent(ProxyComp);
    }
}

void AHktIngamePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CachedClientRule)
    {
        CachedClientRule->BindContext(nullptr, nullptr, nullptr, nullptr);
    }

    CachedClientRule       = nullptr;
    CachedIntentBuilder    = nullptr;
    CachedSelectionPolicy  = nullptr;
    CachedProxySimulator   = nullptr;
    CachedCommandContainer = nullptr;
    CachedWorldPlayer      = nullptr;

    Super::EndPlay(EndPlayReason);
}

void AHktIngamePlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (CachedWorldPlayer)
    {
        CachedWorldPlayer->InvalidatePlayerUidCache();
    }

    UE_LOG(LogHktIngamePlayerController, Log, TEXT("OnRep_PlayerState"));
}

void AHktIngamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput) return;

    if (SubjectAction) EnhancedInput->BindAction(SubjectAction, ETriggerEvent::Started, this, &AHktIngamePlayerController::OnSubjectAction);
    if (TargetAction)  EnhancedInput->BindAction(TargetAction,  ETriggerEvent::Started, this, &AHktIngamePlayerController::OnTargetAction);
    if (ZoomAction)    EnhancedInput->BindAction(ZoomAction,    ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnZoom);

    for (int32 i = 0; i < SlotActions.Num(); ++i)
    {
        if (SlotActions[i]) EnhancedInput->BindAction(SlotActions[i], ETriggerEvent::Started, this, &AHktIngamePlayerController::OnSlotAction, i);
    }
}

// ============================================================================
// 입력 이벤트 — Rule이 내부 캐싱된 컨텍스트 사용 (BindContext 후)
// ============================================================================

void AHktIngamePlayerController::OnSubjectAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    Rule->OnUserEvent_SubjectInputAction();

    if (CachedIntentBuilder)
    {
        // 빈 공간 클릭 시 (Subject 미선택) → 기본 Subject로 복원
        if (CachedIntentBuilder->GetSubjectEntityId() == InvalidEntityId && DefaultSubjectEntityId != InvalidEntityId)
        {
            CachedIntentBuilder->SetSubject(DefaultSubjectEntityId);
        }

        SubjectChangedDelegate.Broadcast(CachedIntentBuilder->GetSubjectEntityId());
        UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnSubjectAction SubjectEntityId=%d"), CachedIntentBuilder->GetSubjectEntityId());
    }
}

void AHktIngamePlayerController::OnTargetAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    Rule->OnUserEvent_TargetInputAction();

    if (CachedIntentBuilder)
    {
        if (CachedIntentBuilder->IsReadyToSubmit() == false)
        {
            CachedIntentBuilder->SetCommand(FGameplayTag::RequestGameplayTag(TEXT("Action.Move.ToLocation")), true);
        }
        CachedIntentBuilder->Submit();

        TargetChangedDelegate.Broadcast(CachedIntentBuilder->GetTargetEntityId());

        if (CachedIntentBuilder->HasPendingSubmit())
        {
            FHktRuntimeEvent Event(CachedIntentBuilder->ConsumePendingSubmit());
            Server_ReceiveIntent(Event);
            IntentSubmittedDelegate.Broadcast(Event);
            UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnTargetAction Submit %s"), *Event.Value.ToString());
        }
        else
        {
            UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnTargetAction TargetEntityId=%d"), CachedIntentBuilder->GetTargetEntityId());
        }
    }
}

void AHktIngamePlayerController::OnSlotAction(const FInputActionValue& Value, int32 SlotIndex)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    Rule->OnUserEvent_CommandInputAction(SlotIndex);

    if (CachedIntentBuilder)
    {
        CommandChangedDelegate.Broadcast(CachedIntentBuilder->GetEventTag());

        if (CachedIntentBuilder->HasPendingSubmit())
        {
            FHktRuntimeEvent Event(CachedIntentBuilder->ConsumePendingSubmit());
            Server_ReceiveIntent(Event);
            IntentSubmittedDelegate.Broadcast(Event);
            UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnSlotAction Submit %s"), *Event.Value.ToString());
        }
        else
        {
            UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnSlotAction SlotIndex=%d EventTag=%s"),
                SlotIndex, *CachedIntentBuilder->GetEventTag().ToString());
        }
    }
}

void AHktIngamePlayerController::OnZoom(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    if (Value.GetValueType() == EInputActionValueType::Axis1D)
    {
        float Delta = Value.Get<float>();
        Rule->OnUserEvent_ZoomInputAction(Delta);
        WheelInputDelegate.Broadcast(Delta);
    }
}

// ============================================================================
// S2C RPC
// ============================================================================

void AHktIngamePlayerController::Client_ReceiveInitialState_Implementation(const FHktRuntimeSimulationState& State)
{
#if ENABLE_HKT_INSIGHTS
    InsightReceivedInitialStateCount++;
#endif

    bIsInitialSync = false;

    IHktClientRule* Rule = GetClientRule();
    if (Rule)
    {
        Rule->OnReceived_InitialState(HktRuntimeConverter::ConvertToWorldState(State));
    }
}

void AHktIngamePlayerController::Client_ReceiveFrameBatch_Implementation(const FHktRuntimeBatch& Batch)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

#if ENABLE_HKT_INSIGHTS
    InsightReceivedBatchCount++;
#endif

    Rule->OnReceived_FrameBatch(static_cast<const FHktSimulationEvent&>(Batch));
}

// ============================================================================
// Tick — ProxySimulator가 생성한 Diff를 소비하여 Presentation에 전달
// ============================================================================

void AHktIngamePlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!CachedProxySimulator || !CachedProxySimulator->IsInitialized()) return;

    if (!bIsInitialSync)
    {
        bIsInitialSync = true;

        // InitialState 수신 후 나의 엔티티를 기본 Subject로 설정
        ResolveDefaultSubject();

        if (CachedProxySimulator)
        {
            FHktWorldView View;
            View.WorldState = &CachedProxySimulator->GetWorldState();
            View.FrameNumber = CachedProxySimulator->GetWorldState().FrameNumber;
            View.bIsInitialSync = true;
            WorldViewUpdatedDelegate.Broadcast(View);
        }
    }

    FHktSimulationDiff Diff;
    if (CachedProxySimulator->ConsumePendingDiff(Diff))
    {
        FHktWorldView View;
        View.WorldState = &CachedProxySimulator->GetWorldState();
        View.FrameNumber = Diff.FrameNumber;
        View.bIsInitialSync = false;
        View.SpawnedEntities = &Diff.SpawnedEntities;
        View.RemovedEntities = &Diff.RemovedEntities;
        View.PropertyDeltas = &Diff.PropertyDeltas;
        View.TagDeltas = &Diff.TagDeltas;
        View.OwnerDeltas = &Diff.OwnerDeltas;
        WorldViewUpdatedDelegate.Broadcast(View);
    }

#if ENABLE_HKT_INSIGHTS
    // 클라이언트 런타임 상태 수집
    {
        const FString Cat = TEXT("Runtime.Client");
        FString NetModeStr;
        switch (GetWorld()->GetNetMode())
        {
        case NM_Standalone:       NetModeStr = TEXT("Standalone"); break;
        case NM_DedicatedServer:  NetModeStr = TEXT("DedicatedServer"); break;
        case NM_ListenServer:     NetModeStr = TEXT("ListenServer"); break;
        default:                  NetModeStr = TEXT("Client"); break;
        }
        HKT_INSIGHT_COLLECT(Cat, TEXT("NetMode"), NetModeStr);
        HKT_INSIGHT_COLLECT(Cat, TEXT("Role"), HasAuthority() ? TEXT("Server") : TEXT("Client"));

        if (CachedIntentBuilder)
        {
            HKT_INSIGHT_COLLECT(Cat, TEXT("Subject"), FString::FromInt(CachedIntentBuilder->GetSubjectEntityId()));
            FGameplayTag Tag = CachedIntentBuilder->GetEventTag();
            HKT_INSIGHT_COLLECT(Cat, TEXT("Command"), Tag.IsValid() ? Tag.ToString() : TEXT("(none)"));
        }

        if (CachedProxySimulator && CachedProxySimulator->IsInitialized())
        {
            const FHktWorldState& WS = CachedProxySimulator->GetWorldState();
            HKT_INSIGHT_COLLECT(Cat, TEXT("ProxyFrame"), FString::Printf(TEXT("%lld"), WS.FrameNumber));
            HKT_INSIGHT_COLLECT(Cat, TEXT("ProxyEntities"), FString::FromInt(WS.GetEntityCount()));
        }

        if (CachedWorldPlayer)
        {
            HKT_INSIGHT_COLLECT(Cat, TEXT("PlayerUid"), FString::Printf(TEXT("%lld"), CachedWorldPlayer->GetPlayerUid()));
        }

        HKT_INSIGHT_COLLECT(Cat, TEXT("SentIntents"), FString::FromInt(InsightSentIntentCount));
        HKT_INSIGHT_COLLECT(Cat, TEXT("ReceivedBatches"), FString::FromInt(InsightReceivedBatchCount));
        HKT_INSIGHT_COLLECT(Cat, TEXT("ReceivedInitialStates"), FString::FromInt(InsightReceivedInitialStateCount));
    }
#endif
}

bool AHktIngamePlayerController::Server_ReceiveIntent_Validate(const FHktRuntimeEvent& Event)
{
    return true;
}

void AHktIngamePlayerController::Server_ReceiveIntent_Implementation(const FHktRuntimeEvent& Event)
{
#if ENABLE_HKT_INSIGHTS
    InsightSentIntentCount++;
#endif

    if (AHktGameMode* GM = GetWorld()->GetAuthGameMode<AHktGameMode>())
    {
        GM->PushIntent(GetPlayerUid(), Event);
    }
}

void AHktIngamePlayerController::ResolveDefaultSubject()
{
    if (!CachedProxySimulator || !CachedProxySimulator->IsInitialized()) return;

    const int64 PlayerUid = GetPlayerUid();
    if (PlayerUid == 0) return;

    const FHktWorldState& WS = CachedProxySimulator->GetWorldState();
    DefaultSubjectEntityId = InvalidEntityId;

    WS.ForEachEntityByOwner(PlayerUid, [this](FHktEntityId Id, int32 /*Slot*/)
    {
        if (DefaultSubjectEntityId == InvalidEntityId)
        {
            DefaultSubjectEntityId = Id;
        }
    });

    if (DefaultSubjectEntityId != InvalidEntityId && CachedIntentBuilder)
    {
        CachedIntentBuilder->SetSubject(DefaultSubjectEntityId);
        SubjectChangedDelegate.Broadcast(DefaultSubjectEntityId);
        UE_LOG(LogHktIngamePlayerController, Log, TEXT("ResolveDefaultSubject: DefaultSubjectEntityId=%d PlayerUid=%lld"), DefaultSubjectEntityId, PlayerUid);
    }
}

IHktClientRule* AHktIngamePlayerController::GetClientRule() const
{
    return CachedClientRule;
}

// ============================================================================
// IHktPlayerInteractionInterface 구현
// ============================================================================

void AHktIngamePlayerController::ExecuteCommand(UObject* CommandData)
{
}

bool AHktIngamePlayerController::GetWorldState(const FHktWorldState*& OutState) const
{
    if (CachedProxySimulator && CachedProxySimulator->IsInitialized())
    {
        OutState = &CachedProxySimulator->GetWorldState();
        return true;
    }
    OutState = nullptr;
    return false;
}

int64 AHktIngamePlayerController::GetPlayerUid() const
{
    return CachedWorldPlayer ? CachedWorldPlayer->GetPlayerUid() : 0;
}
