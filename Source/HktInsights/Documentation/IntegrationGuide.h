// Copyright HKT. All Rights Reserved.

/**
 * HKT Insights 통합 가이드
 * 
 * 이 파일은 기존 HktIntentSubsystem 및 HktCoreSubsystem에
 * 디버그 훅을 추가하는 방법을 보여줍니다.
 */

#pragma once

// =============================================================================
// 1. 헤더 포함
// =============================================================================

// 서브시스템 헤더에 추가:
// #include "HktInsightsDataCollector.h"

// 또는 매크로만 사용할 경우:
// #include "HktInsightsDataCollector.h" // 매크로 정의 포함


// =============================================================================
// 2. HktIntentSubsystem 수정 예제
// =============================================================================

/*
// HktIntentSubsystem.cpp

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#endif

bool UHktIntentSubsystem::AddEvent(const FHktIntentEvent& InEvent)
{
    // 기존 로직
    int32 NewEventId = NextEventId++;
    FHktIntentEvent& StoredEvent = EventHistory.Add_GetRef(InEvent);
    StoredEvent.EventId = NewEventId;
    LatestFrameNumber = FMath::Max(LatestFrameNumber, InEvent.FrameNumber);

    // === HktInsights 훅 추가 ===
#if WITH_HKT_INSIGHTS
    FHktInsightsDataCollector::Get().RecordIntentEvent(
        NewEventId,
        InEvent.EventTag,
        InEvent.SubjectId,
        InEvent.TargetId,
        InEvent.Location,
        EHktInsightsEventState::Pending
    );
#endif
    // === 훅 끝 ===

    return true;
}

void UHktIntentSubsystem::ProcessPendingEvents()
{
    for (FHktIntentEvent& Event : PendingEvents)
    {
        // === HktInsights 훅 추가 (처리 시작) ===
#if WITH_HKT_INSIGHTS
        FHktInsightsDataCollector::Get().UpdateIntentEventState(
            Event.EventId,
            EHktInsightsEventState::Processing
        );
#endif
        // === 훅 끝 ===

        // 기존 처리 로직...
        ProcessEvent(Event);
    }
}

void UHktIntentSubsystem::Commit(int32 ProcessedEventId, const FHktCoreResult& Result)
{
    // 기존 로직...

    // === HktInsights 훅 추가 (완료) ===
#if WITH_HKT_INSIGHTS
    EHktInsightsEventState FinalState = Result.bSuccess 
        ? EHktInsightsEventState::Completed 
        : EHktInsightsEventState::Failed;
    FHktInsightsDataCollector::Get().UpdateIntentEventState(ProcessedEventId, FinalState);
#endif
    // === 훅 끝 ===
}
*/


// =============================================================================
// 3. HktCoreSubsystem (VM 관리) 수정 예제
// =============================================================================

/*
// HktCoreSubsystem.cpp

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#endif

void UHktCoreSubsystem::ExecuteIntentEvent(const FHktIntentEvent& Event)
{
    // 기존 VM 생성 로직
    FHktFlowVM* NewVM = VMPool->Acquire(GetWorld(), &EntityManager, SubjectHandle);
    BuildBytecodeForEvent(*NewVM, Event);

    if (NewVM->Bytecode.Num() > 0)
    {
        int32 VMId = NextVMId++;
        NewVM->DebugVMId = VMId;  // VM 구조체에 디버그 ID 필드 추가 필요
        ActiveVMs.Add(NewVM);

        // === HktInsights 훅 추가 ===
#if WITH_HKT_INSIGHTS
        FHktInsightsDataCollector::Get().RecordVMCreated(
            VMId,
            Event.EventId,
            Event.EventTag,
            NewVM->Bytecode.Num(),
            Event.SubjectId
        );
#endif
        // === 훅 끝 ===
    }
}

void UHktCoreSubsystem::TickActiveVMs(float DeltaTime)
{
    for (int32 i = ActiveVMs.Num() - 1; i >= 0; --i)
    {
        FHktFlowVM* VM = ActiveVMs[i];
        
        // 기존 Tick 로직
        bool bCompleted = VM->Tick(DeltaTime);

        // === HktInsights 훅 추가 (Tick 업데이트) ===
#if WITH_HKT_INSIGHTS
        EHktInsightsVMState VMState;
        if (VM->Regs.bBlocked)
        {
            VMState = EHktInsightsVMState::Blocked;
        }
        else if (VM->bIsMoving)  // 예시: 이동 중 플래그
        {
            VMState = EHktInsightsVMState::Moving;
        }
        else
        {
            VMState = EHktInsightsVMState::Running;
        }

        FHktInsightsDataCollector::Get().RecordVMTick(
            VM->DebugVMId,
            VM->Regs.ProgramCounter,
            VMState,
            GetCurrentOpcodeName(VM)  // Opcode 이름 반환 함수
        );
#endif
        // === 훅 끝 ===

        // VM 완료 처리
        if (bCompleted)
        {
            // === HktInsights 훅 추가 (완료) ===
#if WITH_HKT_INSIGHTS
            FHktInsightsDataCollector::Get().RecordVMCompleted(VM->DebugVMId, true);
#endif
            // === 훅 끝 ===

            VMPool->Release(VM);
            ActiveVMs.RemoveAtSwap(i);
        }
    }
}

// Opcode 이름 반환 헬퍼 함수 예시
FString UHktCoreSubsystem::GetCurrentOpcodeName(FHktFlowVM* VM) const
{
    if (!VM || VM->Regs.ProgramCounter >= VM->Bytecode.Num())
    {
        return TEXT("");
    }

    uint8 Opcode = VM->Bytecode[VM->Regs.ProgramCounter];
    
    // Opcode enum에 따른 이름 반환
    switch (Opcode)
    {
    case 0x01: return TEXT("MOVE_TO");
    case 0x02: return TEXT("PLAY_ANIM");
    case 0x03: return TEXT("WAIT");
    case 0x04: return TEXT("ATTACK");
    case 0x05: return TEXT("USE_SKILL");
    // ... 더 많은 opcode 추가
    default: return FString::Printf(TEXT("OP_%02X"), Opcode);
    }
}
*/


// =============================================================================
// 4. 매크로 사용 예제 (더 간단한 방법)
// =============================================================================

/*
// 매크로를 사용하면 WITH_HKT_INSIGHTS 검사를 자동으로 처리합니다.
// 헤더만 include하면 매크로가 자동으로 빈 코드가 되므로 안전합니다.

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#endif

bool UHktIntentSubsystem::AddEvent(const FHktIntentEvent& InEvent)
{
    int32 NewEventId = NextEventId++;
    // ... 기존 로직 ...

    // 매크로 사용 - Shipping에서는 자동으로 빈 코드
    HKT_INSIGHTS_RECORD_INTENT(NewEventId, InEvent.EventTag, InEvent.SubjectId, InEvent.TargetId, InEvent.Location);

    return true;
}

void UHktIntentSubsystem::Commit(int32 ProcessedEventId, const FHktCoreResult& Result)
{
    // 매크로 사용
    HKT_INSIGHTS_UPDATE_INTENT_STATE(ProcessedEventId, 
        Result.bSuccess ? EHktInsightsEventState::Completed : EHktInsightsEventState::Failed);
}

void UHktCoreSubsystem::ExecuteIntentEvent(const FHktIntentEvent& Event)
{
    // VM 생성 후
    HKT_INSIGHTS_RECORD_VM_CREATED(VMId, Event.EventId, Event.EventTag, BytecodeSize, Event.SubjectId);
}

void UHktCoreSubsystem::TickActiveVMs(float DeltaTime)
{
    // VM Tick 후
    HKT_INSIGHTS_RECORD_VM_TICK(VM->DebugVMId, VM->Regs.ProgramCounter, VMState, OpcodeName);
    
    // VM 완료 시
    HKT_INSIGHTS_RECORD_VM_COMPLETED(VM->DebugVMId, bSuccess);
}
*/


// =============================================================================
// 5. VM 구조체 수정 (디버그 ID 추가)
// =============================================================================

/*
// FHktFlowVM 구조체에 추가할 필드

struct FHktFlowVM
{
    // ... 기존 필드들 ...

#if !UE_BUILD_SHIPPING
    // 디버그용 VM ID
    int32 DebugVMId = INDEX_NONE;
#endif
};
*/


// =============================================================================
// 6. 프로젝트 설정
// =============================================================================

/*
프로젝트의 .uproject 또는 플러그인의 .uplugin 파일에 모듈 추가:

{
    "Modules": [
        {
            "Name": "HktInsights",
            "Type": "DeveloperTool",
            "LoadingPhase": "Default"
        },
        {
            "Name": "HktInsightsEditor",
            "Type": "Editor",
            "LoadingPhase": "Default"
        }
    ]
}

- HktInsights: "DeveloperTool" 타입으로 Shipping 빌드에서 자동 제외
- HktInsightsEditor: "Editor" 타입으로 에디터 빌드에서만 포함

그리고 HktRuntime (또는 HktInsights를 사용하는 모듈)의 Build.cs에 조건부 의존성 추가:

// HktRuntime.Build.cs
if (Target.Configuration != UnrealTargetConfiguration.Shipping)
{
    PrivateDependencyModuleNames.Add("HktInsights");
    PrivateDefinitions.Add("WITH_HKT_INSIGHTS=1");
}
else
{
    PrivateDefinitions.Add("WITH_HKT_INSIGHTS=0");
}

이렇게 설정하면:
- Development/Editor 빌드: HktInsights 포함, 디버그 기록 활성화
- Shipping 빌드: HktInsights 제외, 매크로가 빈 코드로 처리됨
*/
