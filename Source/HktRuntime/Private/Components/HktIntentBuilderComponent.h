// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "Rules/HktClientRule.h"
#include "HktIntentBuilderComponent.generated.h"

/**
 * UHktIntentBuilderComponent
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - 3개 인터페이스를 동시에 구현:
 *     - IHktIntentBuilder:          Subject/Command/Target 설정 + Submit
 *     - IHktSubjectSelectionPolicy: 커서 아래 Selectable 조회
 *     - IHktTargetSelectionPolicy:  커서 아래 Target 조회
 *
 * Rule에서의 사용:
 *   Rule->OnUserEvent_SubjectInputAction(*this, *this);  // Policy + Builder
 *   Rule->OnUserEvent_TargetInputAction(*this, *this);   // Policy + Builder
 *
 * Submit 흐름:
 *   Rule이 IHktIntentBuilder::Submit() 호출
 *   → 내부에서 IntentEvent 생성, PendingSubmit에 저장
 *   → Actor(PlayerController)가 HasPendingSubmit() 확인 후 ConsumePendingSubmit()으로 가져감
 *   → Actor가 Server_ReceiveIntent RPC 발행
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktIntentBuilderComponent
    : public UActorComponent
    , public IHktIntentBuilder
    , public IHktSubjectSelectionPolicy
    , public IHktTargetSelectionPolicy
{
    GENERATED_BODY()

public:
    UHktIntentBuilderComponent();

    // === IHktIntentBuilder 구현 ===

    virtual void SetSubject(FHktEntityId InSubject) override;
    virtual void SetCommand(FGameplayTag InEventTag, bool bInTargetRequired) override;
    virtual void SetTarget(FHktEntityId InTarget, FVector InLocation) override;
    virtual void ResetCommand() override;
    virtual bool IsReadyToSubmit() const override;
    virtual bool Submit() override;

    // === IHktSubjectSelectionPolicy 구현 ===

    virtual FHktEntityId ResolveSubject() const override;

    // === IHktTargetSelectionPolicy 구현 ===

    virtual void ResolveTarget(FHktEntityId& OutEntity, FVector& OutLocation) const override;

    // === Submit 결과 관리 (Actor에서 소비) ===

    /** Submit이 호출되어 대기 중인 Intent가 있는지 */
    bool HasPendingSubmit() const { return bHasPendingSubmit; }

    /** 대기 중인 Intent를 소비 (Actor가 RPC로 전송) */
    FHktIntentEvent ConsumePendingSubmit();

    // === 상태 조회 (Presentation용) ===

    FHktEntityId GetSubjectEntityId() const { return SubjectEntityId; }
    FHktEntityId GetTargetEntityId() const { return TargetEntityId; }
    FVector GetTargetLocation() const { return TargetLocation; }
    FGameplayTag GetEventTag() const { return EventTag; }

private:
    bool GetHitUnderCursor(FHitResult& OutHit) const;
    bool GetSelectableEntityUnderCursor(FHktEntityId& OutEntityId) const;

private:
    FHktEntityId SubjectEntityId = InvalidEntityId;
    FGameplayTag EventTag;
    FHktEntityId TargetEntityId = InvalidEntityId;
    FVector TargetLocation = FVector::ZeroVector;
    bool bTargetRequired = true;

    // Submit 결과 (Actor가 소비)
    bool bHasPendingSubmit = false;
    FHktIntentEvent PendingSubmitEvent;

    // Intent 시퀀스 번호
    static uint32 StaticIntentSequence;
};
