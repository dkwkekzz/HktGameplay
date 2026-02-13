// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktInGamePlayerController.h"
#include "HktPlayerState.h"
#include "HktRuleSubsystem.h"
#include "HktRuleInterfaces.h"
#include "Rules/HktClientRule.h"
#include "Components/HktIntentBuilderComponent.h"
#include "Components/HktDesktopDefaultSelectionPolicy.h"
#include "Components/HktClientSimulatorComponent.h"
#include "Components/HktCommandContainerComponent.h"
#include "Components/HktWorldPlayerComponent.h"
#include "DataAssets/HktInputAction.h"
#include "HktGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#include "HktInsightsRuntimeTypes.h"
#endif

AHktInGamePlayerController::AHktInGamePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void AHktInGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    // 서버 전용: WorldPlayerComponent (IHktWorldPlayer)
    if (HasAuthority())
    {
        WorldPlayerComponent = NewObject<UHktWorldPlayerComponent>(this, TEXT("WorldPlayer"));
        WorldPlayerComponent->RegisterComponent();
    }

    // 클라이언트 전용 컴포넌트
    if (GetWorld()->GetNetMode() == ENetMode::NM_Standalone || !HasAuthority())
    {
        IntentBuilderComponent = NewObject<UHktIntentBuilderComponent>(this, TEXT("IntentBuilder"));
        IntentBuilderComponent->RegisterComponent();

        SelectionPolicyComponent = NewObject<UHktDesktopDefaultSelectionPolicy>(this, TEXT("SelectionPolicy"));
        SelectionPolicyComponent->RegisterComponent();

        ClientSimulatorComponent = NewObject<UHktClientSimulatorComponent>(this, TEXT("ClientSimulator"));
        ClientSimulatorComponent->RegisterComponent();

        CommandContainerComponent = NewObject<UHktCommandContainerComponent>(this, TEXT("CommandContainer"));
        CommandContainerComponent->RegisterComponent();
        CommandContainerComponent->SetSlotActions(SlotActions);
    }

    if (UHktRuleSubsystem* RuleSS = UHktRuleSubsystem::Get(GetWorld()))
    {
        if (!RuleSS->GetClientRule())
        {
            RuleSS->SetClientRule(MakeShared<FHktDefaultClientRule>());
        }
    }

    HKT_INSIGHTS_REGISTER_PROVIDER(this);
}

void AHktInGamePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    HKT_INSIGHTS_UNREGISTER_PROVIDER(this);
    Super::EndPlay(EndPlayReason);
}

void AHktInGamePlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    // PlayerState가 변경되면 컴포넌트의 캐시를 무효화
    if (WorldPlayerComponent)
    {
        WorldPlayerComponent->InvalidatePlayerUidCache();
    }
}

void AHktInGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput) return;

    if (SubjectAction) EnhancedInput->BindAction(SubjectAction, ETriggerEvent::Triggered, this, &AHktInGamePlayerController::OnSubjectAction);
    if (TargetAction) EnhancedInput->BindAction(TargetAction, ETriggerEvent::Triggered, this, &AHktInGamePlayerController::OnTargetAction);
    if (ZoomAction) EnhancedInput->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AHktInGamePlayerController::OnZoom);

    for (int32 i = 0; i < SlotActions.Num(); ++i)
    {
        if (SlotActions[i]) EnhancedInput->BindAction(SlotActions[i], ETriggerEvent::Triggered, this, &AHktInGamePlayerController::OnSlotAction, i);
    }
}

void AHktInGamePlayerController::OnSubjectAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !IntentBuilderComponent || !SelectionPolicyComponent) return;
    Rule->OnUserEvent_SubjectInputAction(*SelectionPolicyComponent, *IntentBuilderComponent);
    SubjectChangedDelegate.Broadcast(IntentBuilderComponent->GetSubjectEntityId());
}

void AHktInGamePlayerController::OnTargetAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !IntentBuilderComponent || !SelectionPolicyComponent) return;
    Rule->OnUserEvent_TargetInputAction(*SelectionPolicyComponent, *IntentBuilderComponent);
    TargetChangedDelegate.Broadcast(IntentBuilderComponent->GetTargetEntityId());

    if (IntentBuilderComponent->HasPendingSubmit())
    {
        FHktRuntimeEvent Event = IntentBuilderComponent->ConsumePendingSubmit();
        Server_ReceiveIntent(Event);
        IntentSubmittedDelegate.Broadcast(Event);
    }
}

void AHktInGamePlayerController::OnSlotAction(const FInputActionValue& Value, int32 SlotIndex)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !IntentBuilderComponent || !CommandContainerComponent) return;
    Rule->OnUserEvent_CommandInputAction(*CommandContainerComponent, SlotIndex, *IntentBuilderComponent);
    CommandChangedDelegate.Broadcast(IntentBuilderComponent->GetEventTag());

    if (IntentBuilderComponent->HasPendingSubmit())
    {
        FHktRuntimeEvent Event = IntentBuilderComponent->ConsumePendingSubmit();
        Server_ReceiveIntent(Event);
        IntentSubmittedDelegate.Broadcast(Event);
    }
}

void AHktInGamePlayerController::OnZoom(const FInputActionValue& Value)
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

void AHktInGamePlayerController::Client_ReceiveFrameBatch_Implementation(const FHktRuntimeBatch& Batch)
{
#if WITH_HKT_INSIGHTS
    InsightReceivedBatchCount++;
    // 암시적 변환을 통해 CoreEvent에 접근
    const FHktSimulationEvent& CoreEvent = Batch;
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
    if (!Rule || !ClientSimulatorComponent) return;
    Rule->OnReceived_FrameBatch(Batch, *ClientSimulatorComponent);
}

void AHktInGamePlayerController::Client_ReceiveInitialState_Implementation(const FHktRuntimeSimulationState& State)
{
#if WITH_HKT_INSIGHTS
    InsightReceivedInitialStateCount++;
    // 암시적 변환을 통해 CoreState에 접근
    const FHktWorldState& CoreState = State;
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
    if (!Rule || !ClientSimulatorComponent) return;
    Rule->OnReceived_InitialSimulationState(State, *ClientSimulatorComponent);
}

bool AHktInGamePlayerController::Server_ReceiveIntent_Validate(const FHktRuntimeEvent& Event)
{
    return Event.IsValid();
}

void AHktInGamePlayerController::Server_ReceiveIntent_Implementation(const FHktRuntimeEvent& Event)
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

IHktClientRule* AHktInGamePlayerController::GetClientRule() const
{
    if (UHktRuleSubsystem* RuleSS = UHktRuleSubsystem::Get(GetWorld()))
    {
        TSharedPtr<IHktClientRule> Rule = RuleSS->GetClientRule();
        return Rule.Get();
    }
    return nullptr;
}

int64 AHktInGamePlayerController::GetPlayerUid() const
{
    if (WorldPlayerComponent)
    {
        return WorldPlayerComponent->GetPlayerUid();
    }
    return 0;
}

// ============================================================================
// IHktInsightProvider 구현
// ============================================================================

#if WITH_HKT_INSIGHTS
void AHktInGamePlayerController::CollectInsightData(FHktInsightSnapshot& OutSnapshot) const
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
    if (IntentBuilderComponent)
    {
        const FString Cat = TEXT("IntentBuilder");
        OutSnapshot.AddInfo(Cat, TEXT("Subject"), FString::FromInt(IntentBuilderComponent->GetSubjectEntityId()));
        OutSnapshot.AddInfo(Cat, TEXT("Target"), FString::FromInt(IntentBuilderComponent->GetTargetEntityId()));

        FGameplayTag Tag = IntentBuilderComponent->GetEventTag();
        OutSnapshot.AddInfo(Cat, TEXT("Command"), Tag.IsValid() ? Tag.ToString() : TEXT("(none)"));
        OutSnapshot.AddInfo(Cat, TEXT("ReadyToSubmit"), IntentBuilderComponent->IsReadyToSubmit() ? TEXT("Yes") : TEXT("No"));
        OutSnapshot.AddInfo(Cat, TEXT("PendingSubmit"), IntentBuilderComponent->HasPendingSubmit() ? TEXT("Yes") : TEXT("No"));
    }

    // === ClientSimulator 상태 ===
    if (ClientSimulatorComponent)
    {
        const FString Cat = TEXT("ClientSimulator");
        bool bInit = ClientSimulatorComponent->IsInitialized();
        OutSnapshot.AddInfo(Cat, TEXT("Initialized"), bInit ? TEXT("Yes") : TEXT("No"));
        if (bInit)
        {
            const FHktWorldState& WorldState = ClientSimulatorComponent->GetSimulationState();
            // 암시적 변환을 통해 CoreState에 접근
            OutSnapshot.AddInfo(Cat, TEXT("LastFrame"), FString::Printf(TEXT("%lld"), WorldState.FrameNumber));
            OutSnapshot.AddInfo(Cat, TEXT("Entities"), FString::FromInt(WorldState.GetEntityCount()));
            //OutSnapshot.AddInfo(Cat, TEXT("ActiveEvents"), FString::FromInt(WorldState.ActiveEvents.Num()));
        }
    }

    // === CommandContainer 상태 ===
    if (CommandContainerComponent)
    {
        const FString Cat = TEXT("CommandContainer");
        OutSnapshot.AddInfo(Cat, TEXT("NumSlots"), FString::FromInt(CommandContainerComponent->GetNumSlots()));
    }

    // === WorldPlayer 상태 (서버) ===
    if (WorldPlayerComponent)
    {
        const FString Cat = TEXT("WorldPlayer");
        OutSnapshot.AddInfo(Cat, TEXT("PlayerUid"), FString::Printf(TEXT("%lld"), WorldPlayerComponent->GetPlayerUid()));
        OutSnapshot.AddInfo(Cat, TEXT("Initialized"), WorldPlayerComponent->IsInitialized() ? TEXT("Yes") : TEXT("No"));
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

