// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPlayerController.h"
#include "HktPlayerState.h"
#include "Components/HktIntentBuilderComponent.h"
#include "Components/HktVisibleStashComponent.h"
#include "Components/HktVMProcessorComponent.h"
#include "HktGameMode.h"
#include "HktInputAction.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#endif

AHktPlayerController::AHktPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void AHktPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
    
    // 클라이언트에서만 VisibleStashComponent 및 VMProcessorComponent 생성
    if (!HasAuthority())
    {
        VisibleStashComponent = NewObject<UHktVisibleStashComponent>(this, TEXT("VisibleStashComponent"));
        VisibleStashComponent->RegisterComponent();

        VMProcessorComponent = NewObject<UHktVMProcessorComponent>(this, TEXT("VMProcessorComponent"));
        VMProcessorComponent->RegisterComponent();

        // VMProcessor를 VisibleStash와 연결
        if (VMProcessorComponent && VisibleStashComponent)
        {
            VMProcessorComponent->Initialize(VisibleStashComponent->GetStashInterface());

            UE_LOG(LogTemp, Log, TEXT("HktPlayerController: Client initialized with VisibleStash and VMProcessor"));
        }
    }

    if (GetWorld()->GetNetMode() == ENetMode::NM_Standalone
        || !HasAuthority())
    {
        IntentBuilderComponent = NewObject<UHktIntentBuilderComponent>(this, TEXT("IntentBuilderComponent"));
        IntentBuilderComponent->RegisterComponent();
    }
}

void AHktPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput) 
        return;

    if (SubjectAction)
        EnhancedInput->BindAction(SubjectAction, ETriggerEvent::Triggered, this, &AHktPlayerController::OnSubjectAction);

    if (TargetAction)
        EnhancedInput->BindAction(TargetAction, ETriggerEvent::Triggered, this, &AHktPlayerController::OnTargetAction);

    if (ZoomAction)
        EnhancedInput->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AHktPlayerController::OnZoom);

    for (int32 i = 0; i < SlotActions.Num(); ++i)
    {
        if (SlotActions[i])
            EnhancedInput->BindAction(SlotActions[i], ETriggerEvent::Triggered, this, &AHktPlayerController::OnSlotAction, i);
    }
}

//-----------------------------------------------------------------------------
// Input Handlers
//-----------------------------------------------------------------------------

void AHktPlayerController::OnSubjectAction(const FInputActionValue& Value)
{
    if (IntentBuilderComponent)
    {
        IntentBuilderComponent->CreateSubjectAction();
        SubjectChangedDelegate.Broadcast(IntentBuilderComponent->GetSubjectEntityId());
    }
}

void AHktPlayerController::OnSlotAction(const FInputActionValue& Value, int32 SlotIndex)
{
    if (!IntentBuilderComponent)
        return;

    IntentBuilderComponent->CreateCommandAction(SlotActions[SlotIndex]->EventTag);
    CommandChangedDelegate.Broadcast(IntentBuilderComponent->GetEventTag());
    
    // Target이 필요 없으면 바로 제출
    if (!IntentBuilderComponent->IsTargetRequired() && IntentBuilderComponent->IsReadyToSubmit())
    {
        SendIntent();
    }
}

void AHktPlayerController::OnTargetAction(const FInputActionValue& Value)
{
    if (!IntentBuilderComponent)
        return;

    IntentBuilderComponent->CreateTargetAction();
    TargetChangedDelegate.Broadcast(IntentBuilderComponent->GetTargetEntityId());
    
    // Target 설정 완료 시 바로 제출
    if (IntentBuilderComponent->IsReadyToSubmit())
    {
        SendIntent();
    }
}

void AHktPlayerController::OnZoom(const FInputActionValue& Value)
{
    if (Value.GetValueType() == EInputActionValueType::Axis1D)
    {
        WheelInputDelegate.Broadcast(Value.Get<float>());
    }
}

bool AHktPlayerController::SendIntent()
{
    if (HasAuthority())
    {
        return false;
    }

    if (!IntentBuilderComponent)
    {
        return false;
    }

    FHktIntentEvent Event;
    if (!IntentBuilderComponent->SubmitIntent(Event))
    {
        return false;
    }

    // HktInsights: Intent 전송 기록 (Sent 상태)
    HKT_INSIGHTS_UPDATE_INTENT_STATE(Event.EventId, EHktInsightsEventState::Sent);

    Server_ReceiveIntent(Event);

    IntentSubmittedDelegate.Broadcast(Event);

    return true;
}

// === C2S RPC ===

bool AHktPlayerController::Server_ReceiveIntent_Validate(const FHktIntentEvent& Event)
{
    return Event.IsValid();
}

void AHktPlayerController::Server_ReceiveIntent_Implementation(const FHktIntentEvent& Event)
{
    // HktInsights: 서버에서 Intent 수신 기록 (Received 상태)
    HKT_INSIGHTS_RECORD_INTENT_WITH_STATE(
        Event.EventId,
        Event.EventTag,
        static_cast<int32>(Event.SourceEntity),
        static_cast<int32>(Event.TargetEntity),
        Event.Location,
        EHktInsightsEventState::Received
    );

    if (AHktGameMode* GM = GetWorld()->GetAuthGameMode<AHktGameMode>())
    {
        GM->PushIntent(Event);
    }
}

// === S2C RPC ===

void AHktPlayerController::SendBatchToOwningClient(const FHktFrameBatch& Batch)
{
    if (HasAuthority())
    {
        Client_ReceiveBatch(Batch);
    }
}

void AHktPlayerController::Client_ReceiveBatch_Implementation(const FHktFrameBatch& Batch)
{
    if (!VisibleStashComponent)
    {
        return;
    }

    IHktStashInterface* Stash = VisibleStashComponent->GetStashInterface();

    // 1. 제거된 엔티티
    for (FHktEntityId EntityId : Batch.RemovedEntities)
    {
        VisibleStashComponent->FreeEntity(EntityId);
        EntityDestroyedDelegate.Broadcast(EntityId);
    }

    // 2. 새 스냅샷 적용
    for (const FHktEntitySnapshot& Snapshot : Batch.Snapshots)
    {
        VisibleStashComponent->ApplyEntitySnapshot(Snapshot);
        EntityCreatedDelegate.Broadcast(Snapshot.EntityId);
    }

    // 3. 이벤트 실행 (VMProcessor)
    if (VMProcessorComponent && VMProcessorComponent->IsInitialized())
    {
        // 모든 이벤트를 VMProcessor에 알림
        VMProcessorComponent->NotifyIntentEvents(Batch.FrameNumber, Batch.Events);
    }
}

// === 소유 엔티티 ===

namespace
{
    constexpr uint16 OwnerPlayerHash = 52;
}

int32 AHktPlayerController::GetMyPlayerHash() const
{
    if (!bPlayerHashCached)
    {
        FString PlayerId;
        if (PlayerState)
        {
            FUniqueNetIdRepl UniqueId = PlayerState->GetUniqueId();
            if (UniqueId.IsValid())
            {
                PlayerId = UniqueId->ToString();
            }
        }
        
        if (PlayerId.IsEmpty())
        {
            PlayerId = FString::Printf(TEXT("Local_%s"), *GetName());
        }
        
        CachedPlayerHash = GetTypeHash(PlayerId);
        bPlayerHashCached = true;
    }
    
    return CachedPlayerHash;
}

bool AHktPlayerController::IsMyEntity(FHktEntityId EntityId) const
{
    IHktStashInterface* Stash = GetStashInterface();
    if (!Stash || !Stash->IsValidEntity(EntityId))
    {
        return false;
    }
    
    int32 EntityOwnerHash = Stash->GetProperty(EntityId, OwnerPlayerHash);
    if (EntityOwnerHash != 0 && EntityOwnerHash == GetMyPlayerHash())
    {
        return true;
    }
    
    return false;
}

TArray<FHktEntityId> AHktPlayerController::GetMyEntities() const
{
    TArray<FHktEntityId> Result;
    
    IHktStashInterface* Stash = GetStashInterface();
    if (!Stash)
    {
        return Result;
    }
    
    int32 MyHash = GetMyPlayerHash();
    
    Stash->ForEachEntity([&](FHktEntityId EntityId)
    {
        int32 EntityOwnerHash = Stash->GetProperty(EntityId, OwnerPlayerHash);
        if (EntityOwnerHash == MyHash)
        {
            Result.Add(EntityId);
        }
    });
    
    return Result;
}

FHktEntityId AHktPlayerController::GetPrimaryEntity() const
{
    TArray<FHktEntityId> MyEntities = GetMyEntities();
    return MyEntities.Num() > 0 ? MyEntities[0] : InvalidEntityId;
}

// === IHktModelProvider 구현 ===

IHktStashInterface* AHktPlayerController::GetStashInterface() const
{
    if (HasAuthority())
    {
        if (AHktGameMode* GM = GetWorld()->GetAuthGameMode<AHktGameMode>())
        {
            return GM->GetStashInterface();
        }
    }

    return VisibleStashComponent ? VisibleStashComponent->GetStashInterface() : nullptr;
}

FHktEntityId AHktPlayerController::GetSelectedSubject() const
{
    return IntentBuilderComponent ? IntentBuilderComponent->GetSubjectEntityId() : InvalidEntityId;
}

FHktEntityId AHktPlayerController::GetSelectedTarget() const
{
    return IntentBuilderComponent ? IntentBuilderComponent->GetTargetEntityId() : InvalidEntityId;
}

FVector AHktPlayerController::GetTargetLocation() const
{
    return IntentBuilderComponent ? IntentBuilderComponent->GetTargetLocation() : FVector::ZeroVector;
}

FGameplayTag AHktPlayerController::GetSelectedCommand() const
{
    return IntentBuilderComponent ? IntentBuilderComponent->GetEventTag() : FGameplayTag();
}

bool AHktPlayerController::IsIntentValid() const
{
    return IntentBuilderComponent ? IntentBuilderComponent->IsValid() : false;
}
