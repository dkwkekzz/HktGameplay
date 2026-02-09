#pragma once

#include "CoreMinimal.h"
#include "HktRuleInterfaces.h"
#include "HktRuntimeTypes.h"
// ============================================================================
// IHktIntentBuilder - Intent 빌더 추상화 (IntentBuilderComponent에서 구현)
// ============================================================================
class IHktIntentBuilder
{
public:
    virtual ~IHktIntentBuilder() = default;
    virtual void SetSubject(FHktEntityId InSubject) = 0;
    virtual void SetCommand(FGameplayTag InEventTag, bool bInTargetRequired) = 0;
    virtual void SetTarget(FHktEntityId InTarget, FVector InLocation) = 0;
    virtual void ResetCommand() = 0;
    virtual bool IsReadyToSubmit() const = 0;
    virtual bool Submit() = 0;
};

// ============================================================================
// IHktSubjectSelectionPolicy - Subject 선택 정책 (커서 히트 등)
// ============================================================================
class IHktSubjectSelectionPolicy
{
public:
    virtual ~IHktSubjectSelectionPolicy() = default;
    virtual FHktEntityId ResolveSubject() const = 0;
};

// ============================================================================
// IHktTargetSelectionPolicy - Target 선택 정책 (커서 히트, 지면 클릭 등)
// ============================================================================
class IHktTargetSelectionPolicy
{
public:
    virtual ~IHktTargetSelectionPolicy() = default;
    virtual void ResolveTarget(FHktEntityId& OutEntity, FVector& OutLocation) const = 0;
};

// ============================================================================
// IHktCommandContainer - 커맨드 슬롯 컨테이너 (SlotActions 등)
// ============================================================================
class IHktCommandContainer
{
public:
    virtual ~IHktCommandContainer() = default;
    virtual FGameplayTag GetEventTagAtSlot(int32 SlotIndex) const = 0;
    virtual bool IsTargetRequiredAtSlot(int32 SlotIndex) const = 0;
    virtual int32 GetNumSlots() const = 0;
};

// ============================================================================
// IHktWorldState - 클라이언트 월드 상태 (VisibleStash 래퍼)
// ============================================================================
class IHktWorldState
{
public:
    virtual ~IHktWorldState() = default;
    virtual void ApplyEntitySnapshot(const FHktEntitySnapshot& Snapshot) = 0;
    virtual void RemoveEntity(FHktEntityId Entity) = 0;
};

// ============================================================================
// FHktDefaultClientRule - IHktClientRule 기본 구현
//
// 기존 AHktPlayerController + AHktEntryPlayerController의 로직을
// Rule 패턴으로 분리한 기본 구현.
//
// 매핑:
//   OnUserEvent_LoginButtonClick     ← HktEntryPlayerController::RequestLogin
//   OnUserEvent_SubjectInputAction   ← HktPlayerController::OnSubjectAction
//   OnUserEvent_TargetInputAction    ← HktPlayerController::OnTargetAction
//   OnUserEvent_CommandInputAction   ← HktPlayerController::OnSlotAction
//   OnUserEvent_ZoomInputAction      ← HktPlayerController::OnZoom
//   OnReceived_FrameBatch            ← HktPlayerController::Client_ReceiveBatch_Implementation
// ============================================================================
class HKTRUNTIME_API FHktDefaultClientRule : public IHktClientRule
{
public:
    FHktDefaultClientRule();
    virtual ~FHktDefaultClientRule();

    virtual void OnUserEvent_LoginButtonClick() override;
    virtual void OnUserEvent_SubjectInputAction(const IHktSubjectSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_TargetInputAction(const IHktTargetSelectionPolicy& InPolicy, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_CommandInputAction(const IHktCommandContainer& InContainer, int32 InSlotIndex, IHktIntentBuilder& InBuilder) override;
    virtual void OnUserEvent_ZoomInputAction(float InDelta) override;
    virtual void OnReceived_FrameBatch(const FHktFrameBatch& InBatch, IHktWorldState& InState, IHktSimulator& InSimulator) override;
};