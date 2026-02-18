// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIngamePlayerController.h"
#include "HktPlayerState.h"
#include "HktClientRuleInterfaces.h"
#include "Rules/HktClientRule.h"
#include "DataAssets/HktInputAction.h"
#include "HktGameMode.h"
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

    if (!ClientRule)
    {
        ClientRule = MakeUnique<FHktDefaultClientRule>();
    }

    // GetComponents를 이용해서 인터페이스들을 캐싱
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
        else if (IHktClientSimulator* ClientSimulator = Cast<IHktClientSimulator>(Comp))
        {
            CachedClientSimulator = ClientSimulator;
        }
        else if (IHktCommandContainer* CommandContainer = Cast<IHktCommandContainer>(Comp))
        {
            CachedCommandContainer = CommandContainer;
            CommandContainer->SetSlotActions(SlotActions);
        }
        else if (IHktWorldPlayer* WorldPlayer = Cast<IHktWorldPlayer>(Comp))
        {
            CachedWorldPlayer = WorldPlayer;
        }
    }

    HKT_INSIGHTS_REGISTER_PROVIDER(this);
}

void AHktIngamePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CachedIntentBuilder = nullptr;
    CachedSelectionPolicy = nullptr;
    CachedClientSimulator = nullptr;
    CachedCommandContainer = nullptr;
    CachedWorldPlayer = nullptr;

    HKT_INSIGHTS_UNREGISTER_PROVIDER(this);
    Super::EndPlay(EndPlayReason);
}

void AHktIngamePlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    // PlayerState가 변경되면 컴포넌트의 캐시를 무효화
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
    if (TargetAction) EnhancedInput->BindAction(TargetAction, ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnTargetAction);
    if (ZoomAction) EnhancedInput->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnZoom);

    for (int32 i = 0; i < SlotActions.Num(); ++i)
    {
        if (SlotActions[i]) EnhancedInput->BindAction(SlotActions[i], ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnSlotAction, i);
    }
}

void AHktIngamePlayerController::OnSubjectAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    IHktUnitSelectionPolicy* Policy = CachedSelectionPolicy;
    IHktIntentBuilder* Builder = CachedIntentBuilder;
    if (!Rule || !Policy || !Builder) return;

    Rule->OnUserEvent_SubjectInputAction(*Policy, *Builder);
    const int32 SubjectEntityId = Builder->GetSubjectEntityId();
    SubjectChangedDelegate.Broadcast(SubjectEntityId);
    
    UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnSubjectAction SubjectEntityId=%d"), SubjectEntityId);
}

void AHktIngamePlayerController::OnTargetAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    IHktUnitSelectionPolicy* Policy = CachedSelectionPolicy;
    IHktIntentBuilder* Builder = CachedIntentBuilder;
    if (!Rule || !Policy || !Builder) return;

    Rule->OnUserEvent_TargetInputAction(*Policy, *Builder);
    const int32 TargetEntityId = Builder->GetTargetEntityId();
    TargetChangedDelegate.Broadcast(TargetEntityId);

    if (Builder->HasPendingSubmit())
    {
        FHktRuntimeEvent Event = Builder->ConsumePendingSubmit();
        Server_ReceiveIntent(Event);
        IntentSubmittedDelegate.Broadcast(Event);
        
        UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnTargetAction Submit %s"), *Event.CoreEvent.ToString());
    }
    else
    {
        UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnTargetAction TargetEntityId=%d"), TargetEntityId);
    }
}

void AHktIngamePlayerController::OnSlotAction(const FInputActionValue& Value, int32 SlotIndex)
{
    IHktClientRule* Rule = GetClientRule();
    IHktCommandContainer* Container = CachedCommandContainer;
    IHktIntentBuilder* Builder = CachedIntentBuilder;
    if (!Rule || !Container || !Builder) return;

    Rule->OnUserEvent_CommandInputAction(*Container, SlotIndex, *Builder);
    const FGameplayTag EventTag = Builder->GetEventTag();
    CommandChangedDelegate.Broadcast(EventTag);

    if (Builder->HasPendingSubmit())
    {
        FHktRuntimeEvent Event = Builder->ConsumePendingSubmit();
        Server_ReceiveIntent(Event);
        IntentSubmittedDelegate.Broadcast(Event);
        
        UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnSlotAction Submit %s"), *Event.CoreEvent.ToString());
    }
    else
    {
        UE_LOG(LogHktIngamePlayerController, Verbose, TEXT("OnSlotAction SlotIndex=%d EventTag=%s"), SlotIndex, *EventTag.ToString());
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

void AHktIngamePlayerController::Client_ReceiveFrameBatch_Implementation(const FHktRuntimeBatch& Batch)
{
#if WITH_HKT_INSIGHTS
    const FHktSimulationEvent& CoreEvent = Batch;
    InsightReceivedBatchCount++;
    // 암시적 변환을 통해 CoreEvent에 접근
    HKT_INSIGHTS_RECORD_PACKET(
        EHktPacketDirection::ServerToClient, 
        EHktPacketType::FrameBatch,
        0, 
        CoreEvent.FrameNumber, 
        CoreEvent.Events.Num(),
        static_cast<int32>(sizeof(FHktRuntimeBatch) + CoreEvent.Events.Num() * sizeof(FHktEvent)),
        FString::Printf(TEXT("FrameBatch: Frame=%lld, Events=%d"), CoreEvent.FrameNumber, CoreEvent.Events.Num())
    );
#endif

    IHktClientRule* Rule = GetClientRule();
    IHktClientSimulator* Simulator = CachedClientSimulator;
    if (!Rule || !Simulator) return;

    Rule->OnReceived_FrameBatch(Batch, *Simulator);
    WorldViewUpdatedDelegate.Broadcast();
}

void AHktIngamePlayerController::Client_ReceiveInitialState_Implementation(const FHktRuntimeSimulationState& State)
{
#if WITH_HKT_INSIGHTS
    const FHktWorldState& CoreState = State;
    InsightReceivedInitialStateCount++;
    // 암시적 변환을 통해 CoreState에 접근
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
    IHktClientSimulator* Simulator = CachedClientSimulator;
    if (!Rule || !Simulator) return;
    
    Rule->OnReceived_InitialSimulationState(State, *Simulator);
    WorldViewUpdatedDelegate.Broadcast();
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
        // 암시적 변환을 통해 CoreEvent에 접근
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
    return ClientRule.Get();
}

// ============================================================================
// IHktPlayerInteractionInterface 구현
// ============================================================================

void AHktIngamePlayerController::ExecuteCommand(UObject* CommandData)
{
    // TODO: 필요 시 Command 라우팅 구현
}

bool AHktIngamePlayerController::GetWorldView(FHktWorldView& OutView) const
{
    if (!CachedClientSimulator || !CachedClientSimulator->IsInitialized())
    {
        return false;
    }
    OutView.WorldState = &CachedClientSimulator->GetSimulationState();
    OutView.IntOverlays.Reset();
    return true;
}

int64 AHktIngamePlayerController::GetPlayerUid() const
{
    if (CachedWorldPlayer)
    {
        return CachedWorldPlayer->GetPlayerUid();
    }
    return 0;
}

// ============================================================================
// IHktInsightProvider 구현
// ============================================================================

#if WITH_HKT_INSIGHTS
void AHktIngamePlayerController::CollectInsightData(FHktInsightSnapshot& OutSnapshot) const
{
    OutSnapshot.ProviderName = GetInsightProviderName();

    // === 기본 정보 ===
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

    // === IntentBuilder 상태 ===
    if (CachedIntentBuilder)
    {
        const FString Cat = TEXT("IntentBuilder");
        const IHktIntentBuilder* Builder = CachedIntentBuilder;
        OutSnapshot.AddInfo(Cat, TEXT("Subject"), FString::FromInt(Builder->GetSubjectEntityId()));
        OutSnapshot.AddInfo(Cat, TEXT("Target"), FString::FromInt(Builder->GetTargetEntityId()));

        FGameplayTag Tag = Builder->GetEventTag();
        OutSnapshot.AddInfo(Cat, TEXT("Command"), Tag.IsValid() ? Tag.ToString() : TEXT("(none)"));
        OutSnapshot.AddInfo(Cat, TEXT("ReadyToSubmit"), Builder->IsReadyToSubmit() ? TEXT("Yes") : TEXT("No"));
        OutSnapshot.AddInfo(Cat, TEXT("PendingSubmit"), Builder->HasPendingSubmit() ? TEXT("Yes") : TEXT("No"));
    }

    // === ClientSimulator 상태 ===
    if (CachedClientSimulator)
    {
        const FString Cat = TEXT("ClientSimulator");
        const IHktClientSimulator* Simulator = CachedClientSimulator;
        bool bInit = Simulator->IsInitialized();
        OutSnapshot.AddInfo(Cat, TEXT("Initialized"), bInit ? TEXT("Yes") : TEXT("No"));
        if (bInit)
        {
            const FHktWorldState& WorldState = Simulator->GetSimulationState();
            // 암시적 변환을 통해 CoreState에 접근
            OutSnapshot.AddInfo(Cat, TEXT("LastFrame"), FString::Printf(TEXT("%lld"), WorldState.FrameNumber));
            OutSnapshot.AddInfo(Cat, TEXT("Entities"), FString::FromInt(WorldState.GetEntityCount()));
            //OutSnapshot.AddInfo(Cat, TEXT("ActiveEvents"), FString::FromInt(WorldState.ActiveEvents.Num()));
        }
    }

    // === CommandContainer 상태 ===
    if (CachedCommandContainer)
    {
        const FString Cat = TEXT("CommandContainer");
        const IHktCommandContainer* Container = CachedCommandContainer;
        OutSnapshot.AddInfo(Cat, TEXT("NumSlots"), FString::FromInt(Container->GetNumSlots()));
    }

    // === WorldPlayer 상태 (서버) ===
    if (CachedWorldPlayer)
    {
        const FString Cat = TEXT("WorldPlayer");
        const IHktWorldPlayer* WorldPlayer = CachedWorldPlayer;
        OutSnapshot.AddInfo(Cat, TEXT("PlayerUid"), FString::Printf(TEXT("%lld"), WorldPlayer->GetPlayerUid()));
        OutSnapshot.AddInfo(Cat, TEXT("Initialized"), WorldPlayer->IsInitialized() ? TEXT("Yes") : TEXT("No"));
    }

    // === RPC 통계 ===
    {
        const FString Cat = TEXT("RPC Stats");
        OutSnapshot.AddInfo(Cat, TEXT("SentIntents"), FString::FromInt(InsightSentIntentCount));
        OutSnapshot.AddInfo(Cat, TEXT("ReceivedBatches"), FString::FromInt(InsightReceivedBatchCount));
        OutSnapshot.AddInfo(Cat, TEXT("ReceivedInitialStates"), FString::FromInt(InsightReceivedInitialStateCount));
    }
}
#endif

