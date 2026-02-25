// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HktCoreDefs.h"
#include "HktWorldState.h"
#include "HktRuntimeTypes.h"
#include "HktClientRuleInterfaces.h"
#include "HktIntentBuilderComponent.generated.h"

/**
 * UHktIntentBuilderComponent
 *
 * 아키텍처:
 *   - 컴포넌트는 인터페이스 구현에 집중
 *   - IHktIntentBuilder 인터페이스 구현:
 *     - Subject/Command/Target 설정 + Submit
 *
 * Rule에서의 사용:
 *   Rule->OnUserEvent_SubjectInputAction(*SelectionPolicy, *this);  // Policy + Builder
 *   Rule->OnUserEvent_TargetInputAction(*SelectionPolicy, *this);   // Policy + Builder
 *
 * Submit 흐름:
 *   Rule이 IHktIntentBuilder::Submit() 호출
 *   → 내부에서 IntentEvent 생성, PendingSubmit에 저장
 *   → Actor(PlayerController)가 HasPendingSubmit() 확인 후 ConsumePendingSubmit()으로 가져감
 *   → Actor가 Server_ReceiveIntent RPC 발행
 */
UCLASS(ClassGroup=(HktRuntime), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktIntentBuilderComponent
    : public UActorComponent
    , public IHktIntentBuilder
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
    virtual FHktEntityId GetSubjectEntityId() const override;
    virtual FHktEntityId GetTargetEntityId() const override;
    virtual FGameplayTag GetEventTag() const override;
    virtual bool HasPendingSubmit() const override;
    virtual FHktEvent ConsumePendingSubmit() override;

    // === 추가 API ===

    FVector GetTargetLocation() const { return TargetLocation; }

private:
    FHktEntityId SubjectEntityId = InvalidEntityId;
    FGameplayTag EventTag;
    FHktEntityId TargetEntityId = InvalidEntityId;
    FVector TargetLocation = FVector::ZeroVector;
    bool bTargetRequired = true;

    // Submit 결과 (Actor가 소비)
    bool bHasPendingSubmit = false;
    FHktRuntimeEvent PendingSubmitEvent;

    // Intent 시퀀스 번호
    static uint32 StaticIntentSequence;
};
