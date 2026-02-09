// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktInGamePlayerController.h"
#include "HktRuleSubsystem.h"
#include "HktRuleInterfaces.h"
#include "Rules/HktClientRule.h"
#include "Components/HktIntentBuilderComponent.h"
#include "Components/HktVisibleStashComponent.h"
#include "Components/HktVMProcessorComponent.h"
#include "Components/HktCommandContainerComponent.h"
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

    // Enhanced Input 설정
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    // 클라이언트 전용 컴포넌트 생성
    if (GetWorld()->GetNetMode() == ENetMode::NM_Standalone || !HasAuthority())
    {
        // IntentBuilder: IHktIntentBuilder + IHktSubjectSelectionPolicy + IHktTargetSelectionPolicy 구현
        IntentBuilderComponent = NewObject<UHktIntentBuilderComponent>(this, TEXT("IntentBuilder"));
        IntentBuilderComponent->RegisterComponent();

        // VisibleStash: 로컬 시뮬레이터의 백엔드
        VisibleStashComponent = NewObject<UHktVisibleStashComponent>(this, TEXT("VisibleStash"));
        VisibleStashComponent->RegisterComponent();

        // VMProcessor: IHktSimulator 래핑
        VMProcessorComponent = NewObject<UHktVMProcessorComponent>(this, TEXT("VMProcessor"));
        VMProcessorComponent->RegisterComponent();

        // CommandContainer: IHktCommandContainer 구현 (SlotActions 래핑)
        CommandContainerComponent = NewObject<UHktCommandContainerComponent>(this, TEXT("CommandContainer"));
        CommandContainerComponent->RegisterComponent();
        CommandContainerComponent->SetSlotActions(SlotActions);

        // VMProcessor 초기화
        if (VMProcessorComponent && VisibleStashComponent)
        {
            VMProcessorComponent->Initialize(VisibleStashComponent->GetStashInterface());
        }
    }

    // 기본 ClientRule 등록 (아직 없다면)
    if (UHktRuleSubsystem* RuleSS = UHktRuleSubsystem::Get(GetWorld()))
    {
        if (!RuleSS->GetClientRule())
        {
            RuleSS->SetClientRule(MakeShared<FHktDefaultClientRule>());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[HktInGamePC] BeginPlay - ClientRule connected"));
}

void AHktInGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput) return;

    if (SubjectAction)
        EnhancedInput->BindAction(SubjectAction, ETriggerEvent::Triggered, this, &AHktInGamePlayerController::OnSubjectAction);

    if (TargetAction)
        EnhancedInput->BindAction(TargetAction, ETriggerEvent::Triggered, this, &AHktInGamePlayerController::OnTargetAction);

    if (ZoomAction)
        EnhancedInput->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AHktInGamePlayerController::OnZoom);

    for (int32 i = 0; i < SlotActions.Num(); ++i)
    {
        if (SlotActions[i])
            EnhancedInput->BindAction(SlotActions[i], ETriggerEvent::Triggered, this, &AHktInGamePlayerController::OnSlotAction, i);
    }
}

// ============================================================================
// 입력 핸들러 → ClientRule 이벤트 발행
// ============================================================================

void AHktInGamePlayerController::OnSubjectAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !IntentBuilderComponent) return;

    // IntentBuilderComponent는 IHktSubjectSelectionPolicy + IHktIntentBuilder를 동시에 구현
    Rule->OnUserEvent_SubjectInputAction(
        *static_cast<IHktSubjectSelectionPolicy*>(IntentBuilderComponent),
        *static_cast<IHktIntentBuilder*>(IntentBuilderComponent)
    );

    SubjectChangedDelegate.Broadcast(IntentBuilderComponent->GetSubjectEntityId());
}

void AHktInGamePlayerController::OnTargetAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !IntentBuilderComponent) return;

    Rule->OnUserEvent_TargetInputAction(
        *static_cast<IHktTargetSelectionPolicy*>(IntentBuilderComponent),
        *static_cast<IHktIntentBuilder*>(IntentBuilderComponent)
    );

    TargetChangedDelegate.Broadcast(IntentBuilderComponent->GetTargetEntityId());

    // IntentBuilder가 Submit 했다면 (Rule 내부에서 Submit 호출)
    // Submit 결과를 서버로 전송
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

    Rule->OnUserEvent_CommandInputAction(
        *static_cast<IHktCommandContainer*>(CommandContainerComponent),
        SlotIndex,
        *static_cast<IHktIntentBuilder*>(IntentBuilderComponent)
    );

    CommandChangedDelegate.Broadcast(IntentBuilderComponent->GetEventTag());

    // Target 불필요한 커맨드에서 Rule이 Submit 했을 수 있음
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

// ============================================================================
// S2C RPC → ClientRule 이벤트 발행
// ============================================================================

void AHktInGamePlayerController::Client_ReceiveFrameBatch_Implementation(const FHktFrameBatch& Batch)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !VMProcessorComponent) return;

    // VMProcessorComponent가 IHktSimulator를 래핑
    IHktSimulator* Simulator = VMProcessorComponent->GetSimulatorInterface();
    if (!Simulator) return;

    Rule->OnReceived_FrameBatch(Batch, *Simulator);
}

void AHktInGamePlayerController::Client_ReceiveInitialState_Implementation(const FHktGroupSimulationState& State)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule || !VMProcessorComponent) return;

    IHktSimulator* Simulator = VMProcessorComponent->GetSimulatorInterface();
    if (!Simulator) return;

    Rule->OnReceived_InitialSimulationState(State, *Simulator);
}

// ============================================================================
// C2S RPC
// ============================================================================

bool AHktInGamePlayerController::Server_ReceiveIntent_Validate(const FHktIntentEvent& Event)
{
    return Event.IsValid();
}

void AHktInGamePlayerController::Server_ReceiveIntent_Implementation(const FHktIntentEvent& Event)
{
    if (AHktGameMode* GM = GetWorld()->GetAuthGameMode<AHktGameMode>())
    {
        // PlayerUid는 서버 측에서 결정 (조작 방지)
        int64 PlayerUid = 0;
        if (PlayerState)
        {
            FUniqueNetIdRepl UniqueId = PlayerState->GetUniqueId();
            if (UniqueId.IsValid())
            {
                PlayerUid = GetTypeHash(UniqueId->ToString());
            }
        }
        GM->PushIntent(PlayerUid, Event);
    }
}

// ============================================================================
// Helpers
// ============================================================================

IHktClientRule* AHktInGamePlayerController::GetClientRule() const
{
    if (UHktRuleSubsystem* RuleSS = UHktRuleSubsystem::Get(GetWorld()))
    {
        TSharedPtr<IHktClientRule> Rule = RuleSS->GetClientRule();
        return Rule.Get();
    }
    return nullptr;
}
