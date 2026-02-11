// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIntentBuilderComponent.h"
#include "HktSelectable.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

uint32 UHktIntentBuilderComponent::StaticIntentSequence = 0;

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

    // IntentEvent 생성 → PendingSubmit에 저장
    PendingSubmitEvent = FHktRuntimeEvent();
    PendingSubmitEvent.EventId = ++StaticIntentSequence;
    PendingSubmitEvent.SourceEntityId = SubjectEntityId;
    PendingSubmitEvent.EventTag = EventTag;
    PendingSubmitEvent.TargetEntityId = TargetEntityId;
    PendingSubmitEvent.Location = TargetLocation;

    bHasPendingSubmit = true;

    UE_LOG(LogTemp, Log, TEXT("[IntentBuilder] Submit: EventId=%d, Tag=%s, Subject=%d, Target=%d"),
        PendingSubmitEvent.EventId, *EventTag.ToString(),
        SubjectEntityId, TargetEntityId);

    // 커맨드 초기화 (Subject 유지)
    ResetCommand();

    return true;
}

// ============================================================================
// IHktSubjectSelectionPolicy 구현
// ============================================================================

FHktEntityId UHktIntentBuilderComponent::ResolveSubject() const
{
    FHktEntityId OutEntity = InvalidEntityId;
    GetSelectableEntityUnderCursor(OutEntity);
    return OutEntity;
}

// ============================================================================
// IHktTargetSelectionPolicy 구현
// ============================================================================

void UHktIntentBuilderComponent::ResolveTarget(FHktEntityId& OutEntity, FVector& OutLocation) const
{
    OutEntity = InvalidEntityId;
    OutLocation = FVector::ZeroVector;

    FHitResult Hit;
    if (!GetHitUnderCursor(Hit))
    {
        return;
    }

    // 엔티티 타겟 시도
    if (IHktSelectable* Selectable = Cast<IHktSelectable>(Hit.GetActor()))
    {
        if (Selectable->IsSelectable())
        {
            OutEntity = Selectable->GetEntityId();
        }
    }

    // 위치는 항상 설정
    OutLocation = Hit.Location;
}

// ============================================================================
// Submit 결과 관리
// ============================================================================

FHktRuntimeEvent UHktIntentBuilderComponent::ConsumePendingSubmit()
{
    bHasPendingSubmit = false;
    return MoveTemp(PendingSubmitEvent);
}

// ============================================================================
// 내부 헬퍼
// ============================================================================

bool UHktIntentBuilderComponent::GetHitUnderCursor(FHitResult& OutHit) const
{
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    if (!Controller)
    {
        return false;
    }

    return Controller->GetHitResultUnderCursor(ECC_Visibility, false, OutHit);
}

bool UHktIntentBuilderComponent::GetSelectableEntityUnderCursor(FHktEntityId& OutEntityId) const
{
    FHitResult Hit;
    if (!GetHitUnderCursor(Hit))
    {
        return false;
    }

    IHktSelectable* Selectable = Cast<IHktSelectable>(Hit.GetActor());
    if (!Selectable || !Selectable->IsSelectable())
    {
        return false;
    }

    OutEntityId = Selectable->GetEntityId();
    return true;
}
