// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "HktIntentBuilderComponent.generated.h"

/**
 * 입력을 조립하여 Intent를 빌드하는 컴포넌트.
 * Subject, Command, Target을 순차적으로 설정하고 제출합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class HKTRUNTIME_API UHktIntentBuilderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHktIntentBuilderComponent();

    //-------------------------------------------------------------------------
    // Action Creation (입력 핸들러에서 호출)
    //-------------------------------------------------------------------------
    
    /** 주체 선택 - 커서 아래 유닛을 Subject로 설정 */
    UFUNCTION(BlueprintCallable, Category = "Hkt|IntentBuilder")
    void CreateSubjectAction();
    
    /** 명령 선택 - 태그로 Command 설정 */
    UFUNCTION(BlueprintCallable, Category = "Hkt|IntentBuilder")
    void CreateCommandAction(FGameplayTag InEventTag);

    /** 대상 선택 - 커서 아래를 Target으로 설정 */
    UFUNCTION(BlueprintCallable, Category = "Hkt|IntentBuilder")
    void CreateTargetAction();

    //-------------------------------------------------------------------------
    // Intent Submission
    //-------------------------------------------------------------------------
    
    /** Intent를 검증하고 서버로 제출 */
    UFUNCTION(BlueprintCallable, Category = "Hkt|IntentBuilder")
    bool SubmitIntent(FHktIntentEvent& OutEvent);

    //-------------------------------------------------------------------------
    // Validation (Blueprint/Debug에서 조회)
    //-------------------------------------------------------------------------
    
    /** Subject와 Command가 유효한지 검사 */
    UFUNCTION(BlueprintPure, Category = "Hkt|IntentBuilder")
    bool IsValid() const;
    
    /** 제출 가능한 상태인지 검사 (Target 필요 여부 포함) */
    UFUNCTION(BlueprintPure, Category = "Hkt|IntentBuilder")
    bool IsReadyToSubmit() const;
    
    /** Target이 필요한 Command인지 */
    UFUNCTION(BlueprintPure, Category = "Hkt|IntentBuilder")
    bool IsTargetRequired() const { return bTargetRequired; }

    //-------------------------------------------------------------------------
    // State Access (IHktControlProvider용)
    //-------------------------------------------------------------------------

    FHktEntityId GetSubjectEntityId() const { return SubjectEntityId; }
    FHktEntityId GetTargetEntityId() const { return TargetEntityId; }
    FVector GetTargetLocation() const { return TargetLocation; }
    FGameplayTag GetEventTag() const { return EventTag; }

    //-------------------------------------------------------------------------
    // Lifecycle
    //-------------------------------------------------------------------------
    
    /** 모든 데이터 초기화 */
    UFUNCTION(BlueprintCallable, Category = "Hkt|IntentBuilder")
    void Reset();
    
    /** Command와 Target만 초기화 (Subject 유지) */
    UFUNCTION(BlueprintCallable, Category = "Hkt|IntentBuilder")
    void ResetCommand();

private:
    bool GetHitUnderCursor(FHitResult& OutHit) const;
    bool GetSelectableEntityUnderCursor(FHktEntityId& OutEntityId) const;

private:
    UPROPERTY(Transient)
    FHktEntityId SubjectEntityId = InvalidEntityId;
    
    UPROPERTY(Transient)
    FGameplayTag EventTag;
    
    UPROPERTY(Transient)
    FHktEntityId TargetEntityId = InvalidEntityId;
    
    UPROPERTY(Transient)
    FVector TargetLocation = FVector::ZeroVector;
    
    UPROPERTY(Transient)
    bool bTargetRequired = true;
};
