// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktInsightsTypes.generated.h"

/**
 * Intent 이벤트 처리 상태
 * 
 * 상태 흐름:
 * [Client] Created → Sent → [Server] Received → Queued → Batched → Dispatched → Pending → Processing → Completed/Failed
 */
UENUM(BlueprintType)
enum class EHktInsightsEventState : uint8
{
    // === 클라이언트 단계 ===
    Created     UMETA(DisplayName = "Created"),      // 클라이언트에서 Intent 빌드 완료
    Sent        UMETA(DisplayName = "Sent"),         // 서버로 RPC 전송됨
    
    // === 서버 수신 단계 ===
    Received    UMETA(DisplayName = "Received"),     // 서버에서 RPC 수신됨
    Queued      UMETA(DisplayName = "Queued"),       // GameMode.CollectedIntents에 추가됨
    
    // === 서버 처리 단계 ===
    Batched     UMETA(DisplayName = "Batched"),      // ProcessFrame에서 배치에 포함됨
    Dispatched  UMETA(DisplayName = "Dispatched"),   // VMProcessor에 전달됨
    
    // === VM 실행 단계 ===
    Pending     UMETA(DisplayName = "Pending"),      // VMProcessor에서 대기 중
    Processing  UMETA(DisplayName = "Processing"),   // VM 실행 중
    
    // === 최종 상태 ===
    Completed   UMETA(DisplayName = "Completed"),    // 완료
    Failed      UMETA(DisplayName = "Failed"),       // 실패
    Cancelled   UMETA(DisplayName = "Cancelled")     // 취소됨
};

/**
 * VM 실행 상태
 */
UENUM(BlueprintType)
enum class EHktInsightsVMState : uint8
{
    Running     UMETA(DisplayName = "Running"),      // 실행 중
    Blocked     UMETA(DisplayName = "Blocked"),      // 대기 중 (애니메이션, 타이머 등)
    Moving      UMETA(DisplayName = "Moving"),       // 이동 중
    Completed   UMETA(DisplayName = "Completed"),    // 완료
    Error       UMETA(DisplayName = "Error")         // 오류
};

/**
 * Intent 이벤트 디버그 정보
 */
USTRUCT(BlueprintType)
struct HKTINSIGHTS_API FHktInsightsIntentEntry
{
    GENERATED_BODY()

    /** 이벤트 고유 ID */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 EventId = 0;

    /** 이벤트가 발생한 프레임 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 FrameNumber = 0;

    /** 이벤트 태그 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGameplayTag EventTag;

    /** Subject 엔티티 ID */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 SubjectId = 0;

    /** Target 엔티티 ID (없으면 INDEX_NONE) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 TargetId = INDEX_NONE;

    /** 이벤트 발생 위치 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector Location = FVector::ZeroVector;

    /** 이벤트 발생 시간 (게임 시작 후 초) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    double Timestamp = 0.0;

    /** 현재 처리 상태 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EHktInsightsEventState State = EHktInsightsEventState::Pending;

    /** 추가 페이로드 정보 (디버그용) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FString PayloadInfo;

    /** 상태를 문자열로 반환 */
    FString GetStateString() const
    {
        switch (State)
        {
        // 클라이언트 단계
        case EHktInsightsEventState::Created:    return TEXT("Created");
        case EHktInsightsEventState::Sent:       return TEXT("Sent");
        // 서버 수신 단계
        case EHktInsightsEventState::Received:   return TEXT("Received");
        case EHktInsightsEventState::Queued:     return TEXT("Queued");
        // 서버 처리 단계
        case EHktInsightsEventState::Batched:    return TEXT("Batched");
        case EHktInsightsEventState::Dispatched: return TEXT("Dispatched");
        // VM 실행 단계
        case EHktInsightsEventState::Pending:    return TEXT("Pending");
        case EHktInsightsEventState::Processing: return TEXT("Processing");
        // 최종 상태
        case EHktInsightsEventState::Completed:  return TEXT("Completed");
        case EHktInsightsEventState::Failed:     return TEXT("Failed");
        case EHktInsightsEventState::Cancelled:  return TEXT("Cancelled");
        default: return TEXT("Unknown");
        }
    }

    /** 상태에 따른 색상 반환 */
    FLinearColor GetStateColor() const
    {
        switch (State)
        {
        // 클라이언트 단계 - Cyan 계열
        case EHktInsightsEventState::Created:    return FLinearColor(0.0f, 0.8f, 0.8f);  // Cyan
        case EHktInsightsEventState::Sent:       return FLinearColor(0.0f, 0.7f, 0.9f);  // Light Blue
        // 서버 수신 단계 - Purple 계열
        case EHktInsightsEventState::Received:   return FLinearColor(0.6f, 0.4f, 0.8f);  // Purple
        case EHktInsightsEventState::Queued:     return FLinearColor(0.7f, 0.5f, 0.9f);  // Light Purple
        // 서버 처리 단계 - Orange 계열
        case EHktInsightsEventState::Batched:    return FLinearColor(1.0f, 0.6f, 0.0f);  // Orange
        case EHktInsightsEventState::Dispatched: return FLinearColor(1.0f, 0.7f, 0.2f);  // Light Orange
        // VM 실행 단계 - Yellow/Blue
        case EHktInsightsEventState::Pending:    return FLinearColor(1.0f, 0.8f, 0.0f);  // Yellow
        case EHktInsightsEventState::Processing: return FLinearColor(0.0f, 0.6f, 1.0f);  // Blue
        // 최종 상태
        case EHktInsightsEventState::Completed:  return FLinearColor(0.0f, 0.8f, 0.2f);  // Green
        case EHktInsightsEventState::Failed:     return FLinearColor(1.0f, 0.2f, 0.2f);  // Red
        case EHktInsightsEventState::Cancelled:  return FLinearColor(0.5f, 0.5f, 0.5f);  // Gray
        default: return FLinearColor::White;
        }
    }

    /** 이벤트 태그를 짧은 문자열로 반환 */
    FString GetShortTagName() const
    {
        FString TagStr = EventTag.ToString();
        int32 LastDotIndex;
        if (TagStr.FindLastChar('.', LastDotIndex))
        {
            return TagStr.RightChop(LastDotIndex + 1);
        }
        return TagStr;
    }
};

/**
 * VM 실행 디버그 정보
 */
USTRUCT(BlueprintType)
struct HKTINSIGHTS_API FHktInsightsVMEntry
{
    GENERATED_BODY()

    /** VM 고유 ID */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 VMId = 0;

    /** 이 VM을 생성한 이벤트 ID */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 SourceEventId = 0;

    /** 소스 이벤트 태그 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGameplayTag SourceEventTag;

    /** 현재 프로그램 카운터 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 ProgramCounter = 0;

    /** 전체 바이트코드 크기 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 BytecodeSize = 0;

    /** 실행 경과 시간 (초) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float ElapsedTime = 0.0f;

    /** VM 상태 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EHktInsightsVMState State = EHktInsightsVMState::Running;

    /** 현재 실행 중인 Opcode 이름 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FString CurrentOpcodeName;

    /** Subject 엔티티 ID */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 SubjectId = 0;

    /** VM 생성 시간 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    double CreationTimestamp = 0.0;

    /** 완료 시간 (완료된 경우) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    double CompletionTimestamp = 0.0;

    /** 진행률 반환 (0.0 ~ 1.0) */
    float GetProgress() const
    {
        return BytecodeSize > 0 ? static_cast<float>(ProgramCounter) / static_cast<float>(BytecodeSize) : 0.0f;
    }

    /** 상태를 문자열로 반환 */
    FString GetStateString() const
    {
        switch (State)
        {
        case EHktInsightsVMState::Running:   return TEXT("Running");
        case EHktInsightsVMState::Blocked:   return TEXT("Blocked");
        case EHktInsightsVMState::Moving:    return TEXT("Moving");
        case EHktInsightsVMState::Completed: return TEXT("Completed");
        case EHktInsightsVMState::Error:     return TEXT("Error");
        default: return TEXT("Unknown");
        }
    }

    /** 상태에 따른 색상 반환 */
    FLinearColor GetStateColor() const
    {
        switch (State)
        {
        case EHktInsightsVMState::Running:   return FLinearColor(0.0f, 0.8f, 0.2f);  // Green
        case EHktInsightsVMState::Blocked:   return FLinearColor(1.0f, 0.5f, 0.0f);  // Orange
        case EHktInsightsVMState::Moving:    return FLinearColor(0.0f, 0.6f, 1.0f);  // Blue
        case EHktInsightsVMState::Completed: return FLinearColor(0.5f, 0.5f, 0.5f);  // Gray
        case EHktInsightsVMState::Error:     return FLinearColor(1.0f, 0.2f, 0.2f);  // Red
        default: return FLinearColor::White;
        }
    }

    /** 진행률 바 문자열 생성 */
    FString GetProgressBarString(int32 BarWidth = 10) const
    {
        float Progress = GetProgress();
        int32 FilledCount = FMath::RoundToInt(Progress * BarWidth);
        int32 EmptyCount = BarWidth - FilledCount;
        
        FString Bar;
        for (int32 i = 0; i < FilledCount; ++i) Bar += TEXT("█");
        for (int32 i = 0; i < EmptyCount; ++i) Bar += TEXT("░");
        
        return FString::Printf(TEXT("%s %d%%"), *Bar, FMath::RoundToInt(Progress * 100));
    }
};

/**
 * 디버그 통계 정보
 */
USTRUCT(BlueprintType)
struct HKTINSIGHTS_API FHktInsightsStats
{
    GENERATED_BODY()

    /** 총 기록된 이벤트 수 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 TotalEventCount = 0;

    /** 현재 대기 중인 이벤트 수 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 PendingEventCount = 0;

    /** 현재 활성 VM 수 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 ActiveVMCount = 0;

    /** 완료된 VM 수 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 CompletedVMCount = 0;

    /** 실패한 이벤트 수 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 FailedEventCount = 0;

    /** 평균 VM 실행 시간 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float AverageVMExecutionTime = 0.0f;
};
