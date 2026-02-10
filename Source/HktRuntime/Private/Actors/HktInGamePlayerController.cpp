// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktInGamePlayerController.h"
#include "HktPlayerState.h"
#include "HktRuleSubsystem.h"
#include "HktRuleInterfaces.h"
#include "Rules/HktClientRule.h"
#include "Components/HktIntentBuilderComponent.h"
#include "Components/HktClientSimulatorComponent.h"
#include "Components/HktCommandContainerComponent.h"
#include "Components/HktWorldPlayerComponent.h"
#include "DataAssets/HktInputAction.h"
#include "HktGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

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
    if (!Rule || !IntentBuilderComponent) return;
    Rule->OnUserEvent_SubjectInputAction(*IntentBuilderComponent, *IntentBuilderComponent);
    SubjectChangedDelegate.Broadcast(IntentBuilderComponent->GetSubjectEntityId());
}

void AHktInGamePlayerController::OnTargetAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !IntentBuilderComponent) return;
    Rule->OnUserEvent_TargetInputAction(*IntentBuilderComponent, *IntentBuilderComponent);
    TargetChangedDelegate.Broadcast(IntentBuilderComponent->GetTargetEntityId());

    if (IntentBuilderComponent->HasPendingSubmit())
    {
        FHktIntentEvent Event = IntentBuilderComponent->ConsumePendingSubmit();
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
        FHktIntentEvent Event = IntentBuilderComponent->ConsumePendingSubmit();
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

void AHktInGamePlayerController::Client_ReceiveFrameBatch_Implementation(const FHktFrameBatch& Batch)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !ClientSimulatorComponent) return;
    Rule->OnReceived_FrameBatch(Batch, *ClientSimulatorComponent);
}

void AHktInGamePlayerController::Client_ReceiveInitialState_Implementation(const FHktGroupSimulationState& State)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !ClientSimulatorComponent) return;
    Rule->OnReceived_InitialSimulationState(State, *ClientSimulatorComponent);
}

bool AHktInGamePlayerController::Server_ReceiveIntent_Validate(const FHktIntentEvent& Event)
{
    return Event.IsValid();
}

void AHktInGamePlayerController::Server_ReceiveIntent_Implementation(const FHktIntentEvent& Event)
{
    if (AHktGameMode* GM = GetWorld()->GetAuthGameMode<AHktGameMode>())
    {
        int64 PlayerUid = 0;
        if (PlayerState)
        {
            FUniqueNetIdRepl UniqueId = PlayerState->GetUniqueId();
            if (UniqueId.IsValid()) { PlayerUid = GetTypeHash(UniqueId->ToString()); }
        }
        GM->PushIntent(PlayerUid, Event);
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
