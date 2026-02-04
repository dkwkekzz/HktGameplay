// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIntentBuilderComponent.h"
#include "HktSelectable.h"
#include "HktCoreInterfaces.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#endif

//=============================================================================
// UHktIntentBuilderComponent
//=============================================================================

UHktIntentBuilderComponent::UHktIntentBuilderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

//-----------------------------------------------------------------------------
// Action Creation
//-----------------------------------------------------------------------------

void UHktIntentBuilderComponent::CreateSubjectAction()
{
    if (GetSelectableEntityUnderCursor(SubjectEntityId))
    {
        UE_LOG(LogTemp, Log, TEXT("[HktIntentBuilder] Subject selected: EntityId=%d"), SubjectEntityId.RawValue);
        
        // 새 Subject 선택 시 Command/Target 초기화
        ResetCommand();
    }
}

void UHktIntentBuilderComponent::CreateCommandAction(FGameplayTag InEventTag)
{
    if (SubjectEntityId == InvalidEntityId)
    {
        return;
    }

    EventTag = InEventTag;

    // Target 초기화
    TargetEntityId = InvalidEntityId;
    TargetLocation = FVector::ZeroVector;
}

void UHktIntentBuilderComponent::CreateTargetAction()
{
    if (GetSelectableEntityUnderCursor(TargetEntityId))
    {
        UE_LOG(LogTemp, Log, TEXT("[HktIntentBuilder] Target selected: EntityId=%d"), TargetEntityId.RawValue);
    }
}

//-----------------------------------------------------------------------------
// Intent Submission
//-----------------------------------------------------------------------------

bool UHktIntentBuilderComponent::SubmitIntent(FHktIntentEvent& OutEvent)
{
    // 1. Validate readiness
    if (!IsReadyToSubmit())
    {
        UE_LOG(LogTemp, Warning, TEXT("[HktIntentBuilder] Cannot submit: Intent is not ready"));
        return false;
    }

    // 2. Build Intent Event
    static uint32 StaticIntentSequence = 0;
    
    OutEvent.EventId = ++StaticIntentSequence;
    OutEvent.SourceEntity = SubjectEntityId;
    OutEvent.EventTag = EventTag;
    OutEvent.TargetEntity = TargetEntityId;
    OutEvent.Location = TargetLocation;
    OutEvent.bIsGlobal = false;

    UE_LOG(LogTemp, Log, TEXT("[HktIntentBuilder] Intent submitted: Tag=%s, EventId=%d, Subject=%d, Target=%d"), 
        *EventTag.ToString(), OutEvent.EventId, SubjectEntityId.RawValue, TargetEntityId.RawValue);

    // HktInsights: Intent 생성 기록 (Created 상태)
    HKT_INSIGHTS_RECORD_INTENT_WITH_STATE(
        OutEvent.EventId,
        OutEvent.EventTag,
        static_cast<int32>(OutEvent.SourceEntity),
        static_cast<int32>(OutEvent.TargetEntity),
        OutEvent.Location,
        EHktInsightsEventState::Created
    );

    // 3. Reset command for next action (keep subject)
    ResetCommand();

    return true;
}

//-----------------------------------------------------------------------------
// Validation
//-----------------------------------------------------------------------------

bool UHktIntentBuilderComponent::IsValid() const
{
    return SubjectEntityId != InvalidEntityId && EventTag.IsValid();
}

bool UHktIntentBuilderComponent::IsReadyToSubmit() const
{
    if (!IsValid())
    {
        return false;
    }

    // Target이 필요한 경우 위치가 설정되어 있어야 함
    if (bTargetRequired)
    {
        return !TargetLocation.IsZero();
    }

    return true;
}

//-----------------------------------------------------------------------------
// Lifecycle
//-----------------------------------------------------------------------------

void UHktIntentBuilderComponent::Reset()
{
    SubjectEntityId = InvalidEntityId;
    EventTag = FGameplayTag();
    TargetEntityId = InvalidEntityId;
    TargetLocation = FVector::ZeroVector;
    bTargetRequired = true;
}

void UHktIntentBuilderComponent::ResetCommand()
{
    EventTag = FGameplayTag();
    TargetEntityId = InvalidEntityId;
    TargetLocation = FVector::ZeroVector;
    bTargetRequired = true;
}

//-----------------------------------------------------------------------------
// Internal Helpers
//-----------------------------------------------------------------------------

bool UHktIntentBuilderComponent::GetHitUnderCursor(FHitResult& OutHit) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // Owner가 PlayerController인 경우 직접 사용
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    if (!Controller)
    {
        return false;
    }

    if (!Controller->GetHitResultUnderCursor(ECC_Visibility, false, OutHit))
    {
        return false;
    }

    return true;
}

bool UHktIntentBuilderComponent::GetSelectableEntityUnderCursor(FHktEntityId& OutEntityId) const
{
    FHitResult Hit;
    if (!GetHitUnderCursor(Hit))
    {
        return false;
    }

    IHktSelectable* Selectable = Cast<IHktSelectable>(Hit.GetActor());
    if (!Selectable)
    {
        return false;
    }

    if (!Selectable->IsSelectable())
    {
        return false;
    }

    OutEntityId = Selectable->GetEntityId();
    return true;
}
