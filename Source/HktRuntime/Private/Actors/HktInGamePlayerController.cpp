// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIngamePlayerController.h"
#include "HktPlayerState.h"
#include "HktClientRuleInterfaces.h"
#include "HktGameMode.h"
#include "HktRuntimeConverter.h"
#include "HktRuntimeTypes.h"
#include "HktWorldView.h"
#include "DataAssets/HktInputAction.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#include "HktInsightsRuntimeTypes.h"
#endif

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
    // (AdvanceLocalFrame + ProcessPendingServerBatches → Diff 생성 → Tick에서 소비)
    if (UActorComponent* ProxyComp = Cast<UActorComponent>(CachedProxySimulator))
    {
        AddTickPrerequisiteComponent(ProxyComp);
    }

    HKT_INSIGHTS_REGISTER_PROVIDER(this);
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

    HKT_INSIGHTS_UNREGISTER_PROVIDER(this);
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

    if (SubjectAction) EnhancedInput->BindAction(SubjectAction, ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnSubjectAction);
    if (TargetAction)  EnhancedInput->BindAction(TargetAction,  ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnTargetAction);
    if (ZoomAction)    EnhancedInput->BindAction(ZoomAction,    ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnZoom);

    for (int32 i = 0; i < SlotActions.Num(); ++i)
    {
        if (SlotActions[i]) EnhancedInput->BindAction(SlotActions[i], ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnSlotAction, i);
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
        TargetChangedDelegate.Broadcast(CachedIntentBuilder->GetTargetEntityId());

        if (CachedIntentBuilder->HasPendingSubmit())
        {
            FHktRuntimeEvent Event(CachedIntentBuilder->ConsumePendingSubmit());
            Server_ReceiveIntent(Event);
            IntentSubmittedDelegate.Broadcast(Event);
            UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnTargetAction Submit %s"), *Event.CoreEvent.ToString());
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
            UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnSlotAction Submit %s"), *Event.CoreEvent.ToString());
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
#if WITH_HKT_INSIGHTS
    const FHktWorldState& CoreState = State;
    InsightReceivedInitialStateCount++;
    HKT_INSIGHTS_RECORD_PACKET(
        EHktPacketDirection::ServerToClient,
        EHktPacketType::InitialState,
        0,
        CoreState.FrameNumber,
        CoreState.GetEntityCount(),
        static_cast<int32>(sizeof(FHktRuntimeSimulationState) + CoreState.GetEntityCount() * sizeof(FHktEntityState)),
        FString::Printf(TEXT("InitialState: Entities=%d"), CoreState.GetEntityCount())
    );
#endif

    IHktClientRule* Rule = GetClientRule();
    if (Rule)
    {
        // Rule이 내부 캐싱된 Simulator에 전달
        Rule->OnReceived_InitialState(HktRuntimeConverter::ConvertToWorldState(State));
    }

    if (CachedProxySimulator)
    {
        FHktWorldView View;
        View.WorldState    = &CachedProxySimulator->GetWorldState();
        View.FrameNumber   = CachedProxySimulator->GetWorldState().FrameNumber;
        View.bIsInitialSync = true;
        WorldViewUpdatedDelegate.Broadcast(View);
    }
}

void AHktIngamePlayerController::Client_ReceiveFrameBatch_Implementation(const FHktRuntimeBatch& Batch)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    // 서버 Batch를 큐에 적재만 함 — Diff 처리는 Tick에서 ConsumePendingDiff로
    Rule->OnReceived_FrameBatch(static_cast<const FHktSimulationEvent&>(Batch));
}

// ============================================================================
// Tick — ProxySimulator가 생성한 Diff를 소비하여 Presentation에 전달
// ============================================================================

void AHktIngamePlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!CachedProxySimulator || !CachedProxySimulator->IsInitialized()) return;

    FHktSimulationDiff Diff;
    if (CachedProxySimulator->ConsumePendingDiff(Diff))
    {
        FHktWorldView View;
        View.WorldState      = &CachedProxySimulator->GetWorldState();
        View.FrameNumber     = Diff.FrameNumber;
        View.bIsInitialSync  = false;
        View.SpawnedEntities = &Diff.SpawnedEntities;
        View.RemovedEntities = &Diff.RemovedEntities;
        View.PropertyDeltas  = &Diff.PropertyDeltas;
        WorldViewUpdatedDelegate.Broadcast(View);
    }
}

bool AHktIngamePlayerController::Server_ReceiveIntent_Validate(const FHktRuntimeEvent& Event)
{
    return Event.IsValid();
}

void AHktIngamePlayerController::Server_ReceiveIntent_Implementation(const FHktRuntimeEvent& Event)
{
#if WITH_HKT_INSIGHTS
    InsightSentIntentCount++;
    {
        const FHktEvent& CoreEvent = Event;
        HKT_INSIGHTS_RECORD_PACKET(
            EHktPacketDirection::ClientToServer,
            EHktPacketType::Intent,
            GetPlayerUid(),
            0,
            1,
            static_cast<int32>(sizeof(FHktRuntimeEvent)),
            FString::Printf(TEXT("Intent: %s Src=%d→Tgt=%d"), *CoreEvent.EventTag.ToString(), CoreEvent.SourceEntity, CoreEvent.TargetEntity)
        );
    }
#endif

    if (AHktGameMode* GM = GetWorld()->GetAuthGameMode<AHktGameMode>())
    {
        GM->PushIntent(GetPlayerUid(), Event);
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

// ============================================================================
// IHktInsightProvider 구현
// ============================================================================

#if WITH_HKT_INSIGHTS
void AHktIngamePlayerController::CollectInsightData(FHktInsightSnapshot& OutSnapshot) const
{
    OutSnapshot.ProviderName = GetInsightProviderName();

    {
        const FString Cat = TEXT("PlayerController");
        OutSnapshot.AddInfo(Cat, TEXT("Name"), GetName());
        OutSnapshot.AddInfo(Cat, TEXT("Role"), HasAuthority() ? TEXT("Server") : TEXT("Client"));

        FString NetModeStr;
        switch (GetWorld()->GetNetMode())
        {
        case NM_Standalone:       NetModeStr = TEXT("Standalone"); break;
        case NM_DedicatedServer:  NetModeStr = TEXT("DedicatedServer"); break;
        case NM_ListenServer:     NetModeStr = TEXT("ListenServer"); break;
        default:                  NetModeStr = TEXT("Client"); break;
        }
        OutSnapshot.AddInfo(Cat, TEXT("NetMode"), NetModeStr);
    }

    if (CachedIntentBuilder)
    {
        const FString Cat = TEXT("IntentBuilder");
        OutSnapshot.AddInfo(Cat, TEXT("Subject"), FString::FromInt(CachedIntentBuilder->GetSubjectEntityId()));
        OutSnapshot.AddInfo(Cat, TEXT("Target"),  FString::FromInt(CachedIntentBuilder->GetTargetEntityId()));
        FGameplayTag Tag = CachedIntentBuilder->GetEventTag();
        OutSnapshot.AddInfo(Cat, TEXT("Command"),       Tag.IsValid() ? Tag.ToString() : TEXT("(none)"));
        OutSnapshot.AddInfo(Cat, TEXT("ReadyToSubmit"), CachedIntentBuilder->IsReadyToSubmit() ? TEXT("Yes") : TEXT("No"));
        OutSnapshot.AddInfo(Cat, TEXT("PendingSubmit"), CachedIntentBuilder->HasPendingSubmit() ? TEXT("Yes") : TEXT("No"));
    }

    if (CachedProxySimulator)
    {
        const FString Cat = TEXT("ProxySimulator");
        bool bInit = CachedProxySimulator->IsInitialized();
        OutSnapshot.AddInfo(Cat, TEXT("Initialized"), bInit ? TEXT("Yes") : TEXT("No"));
        if (bInit)
        {
            const FHktWorldState& WS = CachedProxySimulator->GetWorldState();
            OutSnapshot.AddInfo(Cat, TEXT("LastFrame"), FString::Printf(TEXT("%lld"), WS.FrameNumber));
            OutSnapshot.AddInfo(Cat, TEXT("Entities"),  FString::FromInt(WS.GetEntityCount()));
        }
    }

    if (CachedCommandContainer)
    {
        const FString Cat = TEXT("CommandContainer");
        OutSnapshot.AddInfo(Cat, TEXT("NumSlots"), FString::FromInt(CachedCommandContainer->GetNumSlots()));
    }

    if (CachedWorldPlayer)
    {
        const FString Cat = TEXT("WorldPlayer");
        OutSnapshot.AddInfo(Cat, TEXT("PlayerUid"),   FString::Printf(TEXT("%lld"), CachedWorldPlayer->GetPlayerUid()));
        OutSnapshot.AddInfo(Cat, TEXT("Initialized"), CachedWorldPlayer->IsInitialized() ? TEXT("Yes") : TEXT("No"));
    }

    {
        const FString Cat = TEXT("RPC Stats");
        OutSnapshot.AddInfo(Cat, TEXT("SentIntents"),          FString::FromInt(InsightSentIntentCount));
        OutSnapshot.AddInfo(Cat, TEXT("ReceivedBatches"),      FString::FromInt(InsightReceivedBatchCount));
        OutSnapshot.AddInfo(Cat, TEXT("ReceivedInitialStates"),FString::FromInt(InsightReceivedInitialStateCount));
    }
}
#endif
