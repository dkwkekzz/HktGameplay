// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIntentBuilderComponent.h"
#include "HktCoreEventLog.h"

UHktIntentBuilderComponent::UHktIntentBuilderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================================
// IHktIntentBuilder 구현
// ============================================================================

void UHktIntentBuilderComponent::SetSubject(FHktEntityId InSubject)
{
    SubjectEntityId = InSubject;
    UE_LOG(LogTemp, Log, TEXT("[IntentBuilder] Subject set: %d"), SubjectEntityId);
}

void UHktIntentBuilderComponent::SetCommand(FGameplayTag InEventTag, bool bInTargetRequired)
{
    EventTag = InEventTag;
    bTargetRequired = bInTargetRequired;

    // 커맨드 변경 시 Target 초기화
    TargetEntityId = InvalidEntityId;
    TargetLocation = FVector::ZeroVector;
}

void UHktIntentBuilderComponent::SetTarget(FHktEntityId InTarget, FVector InLocation)
{
    TargetEntityId = InTarget;
    TargetLocation = InLocation;
}

void UHktIntentBuilderComponent::ResetCommand()
{
    EventTag = FGameplayTag();
    TargetEntityId = InvalidEntityId;
    TargetLocation = FVector::ZeroVector;
    bTargetRequired = true;
}

bool UHktIntentBuilderComponent::IsReadyToSubmit() const
{
    if (SubjectEntityId == InvalidEntityId || !EventTag.IsValid())
    {
        return false;
    }

    if (bTargetRequired)
    {
        return !TargetLocation.IsZero();
    }

    return true;
}

bool UHktIntentBuilderComponent::Submit()
{
    if (!IsReadyToSubmit())
    {
        return false;
    }

    // Core 구조체로 생성
    FHktEvent CoreEvent;
    CoreEvent.EventId = 0;  // 서버가 할당
    CoreEvent.SourceEntity = SubjectEntityId;
    CoreEvent.EventTag = EventTag;
    CoreEvent.TargetEntity = TargetEntityId;
    CoreEvent.Location = TargetLocation;
    CoreEvent.Param0 = 0;
    CoreEvent.Param1 = 0;

    PendingSubmitEvent = CoreEvent; // 복사 생성자 사용

    bHasPendingSubmit = true;

    HKT_EVENT_LOG_TAG("Runtime.Intent",
        FString::Printf(TEXT("Submit Tag=%s Subject=%d Target=%d Loc=(%.0f,%.0f,%.0f)"),
            *EventTag.ToString(), SubjectEntityId, TargetEntityId,
            TargetLocation.X, TargetLocation.Y, TargetLocation.Z),
        SubjectEntityId, EventTag);

    // 커맨드 초기화 (Subject 유지)
    ResetCommand();

    return true;
}

FHktEntityId UHktIntentBuilderComponent::GetSubjectEntityId() const
{
    return SubjectEntityId;
}

FHktEntityId UHktIntentBuilderComponent::GetTargetEntityId() const
{
    return TargetEntityId;
}

FGameplayTag UHktIntentBuilderComponent::GetEventTag() const
{
    return EventTag;
}

bool UHktIntentBuilderComponent::HasPendingSubmit() const
{
    return bHasPendingSubmit;
}

FHktEvent UHktIntentBuilderComponent::ConsumePendingSubmit()
{
    bHasPendingSubmit = false;
    return PendingSubmitEvent.Value;
}
