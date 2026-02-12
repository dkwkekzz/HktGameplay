// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktDesktopDefaultSelectionPolicy.h"
#include "HktSelectable.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UHktDesktopDefaultSelectionPolicy::UHktDesktopDefaultSelectionPolicy()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================================
// IHktUnitSelectionPolicy 구현
// ============================================================================

FHktEntityId UHktDesktopDefaultSelectionPolicy::ResolveSubject() const
{
    FHktEntityId OutEntity = InvalidEntityId;
    GetSelectableEntityUnderCursor(OutEntity);
    return OutEntity;
}

void UHktDesktopDefaultSelectionPolicy::ResolveTarget(FHktEntityId& OutEntity, FVector& OutLocation) const
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
// 내부 헬퍼
// ============================================================================

bool UHktDesktopDefaultSelectionPolicy::GetHitUnderCursor(FHitResult& OutHit) const
{
    APlayerController* Controller = Cast<APlayerController>(GetOwner());
    if (!Controller)
    {
        return false;
    }

    return Controller->GetHitResultUnderCursor(ECC_Visibility, false, OutHit);
}

bool UHktDesktopDefaultSelectionPolicy::GetSelectableEntityUnderCursor(FHktEntityId& OutEntityId) const
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
