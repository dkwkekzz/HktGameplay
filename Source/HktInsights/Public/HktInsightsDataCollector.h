// Copyright HKT. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktInsightsTypes.h"

DECLARE_MULTICAST_DELEGATE(FOnHktInsightsDataUpdated);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHktInsightsIntentRecorded, const FHktInsightsIntentEntry&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHktInsightsVMCreated, const FHktInsightsVMEntry&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHktInsightsVMCompleted, const FHktInsightsVMEntry&);

/**
 * HKT 디버그 데이터 수집기
 * 
 * Intent 이벤트와 VM 실행 정보를 수집하고 저장합니다.
 * 스레드 세이프하며, UI 독립적인 데이터 레이어입니다.
 * 
 * 사용법:
 * - Recording API: 서브시스템에서 호출하여 데이터 기록
 * - Query API: UI에서 호출하여 데이터 조회
 */
class HKTINSIGHTS_API FHktInsightsDataCollector
{
public:
    /** 싱글톤 인스턴스 반환 */
    static FHktInsightsDataCollector& Get();

    // ========== Recording API (서브시스템에서 호출) ==========

    /**
     * Intent 이벤트 기록
     * @param EventId 이벤트 ID
     * @param EventTag 이벤트 태그
     * @param SubjectId Subject 엔티티 ID
     * @param TargetId Target 엔티티 ID (없으면 INDEX_NONE)
     * @param Location 이벤트 발생 위치
     * @param State 초기 상태
     */
    void RecordIntentEvent(
        int32 EventId,
        const FGameplayTag& EventTag,
        int32 SubjectId,
        int32 TargetId = INDEX_NONE,
        const FVector& Location = FVector::ZeroVector,
        EHktInsightsEventState State = EHktInsightsEventState::Pending);

    /**
     * Intent 이벤트 상태 업데이트
     * @param EventId 업데이트할 이벤트 ID
     * @param NewState 새 상태
     */
    void UpdateIntentEventState(int32 EventId, EHktInsightsEventState NewState);

    /**
     * VM 생성 기록
     * @param VMId VM 고유 ID
     * @param SourceEventId 이 VM을 생성한 이벤트 ID
     * @param SourceEventTag 소스 이벤트 태그
     * @param BytecodeSize 전체 바이트코드 크기
     * @param SubjectId Subject 엔티티 ID
     */
    void RecordVMCreated(
        int32 VMId,
        int32 SourceEventId,
        const FGameplayTag& SourceEventTag,
        int32 BytecodeSize,
        int32 SubjectId);

    /**
     * VM Tick 업데이트
     * @param VMId VM ID
     * @param ProgramCounter 현재 PC
     * @param State 현재 상태
     * @param CurrentOpcodeName 현재 실행 중인 Opcode 이름
     */
    void RecordVMTick(
        int32 VMId,
        int32 ProgramCounter,
        EHktInsightsVMState State,
        const FString& CurrentOpcodeName);

    /**
     * VM 완료 기록
     * @param VMId 완료된 VM ID
     * @param bSuccess 성공 여부
     */
    void RecordVMCompleted(int32 VMId, bool bSuccess = true);

    // ========== Query API (UI에서 호출) ==========

    /**
     * 최근 Intent 이벤트 목록 반환
     * @param MaxCount 최대 반환 개수
     * @return 최근 이벤트 목록 (최신 것이 먼저)
     */
    TArray<FHktInsightsIntentEntry> GetRecentIntentEvents(int32 MaxCount = 100) const;

    /**
     * 특정 상태의 Intent 이벤트 목록 반환
     * @param State 필터링할 상태
     * @param MaxCount 최대 반환 개수
     */
    TArray<FHktInsightsIntentEntry> GetIntentEventsByState(EHktInsightsEventState State, int32 MaxCount = 100) const;

    /**
     * 현재 활성 VM 목록 반환
     */
    TArray<FHktInsightsVMEntry> GetActiveVMs() const;

    /**
     * 최근 완료된 VM 목록 반환
     * @param MaxCount 최대 반환 개수
     */
    TArray<FHktInsightsVMEntry> GetRecentCompletedVMs(int32 MaxCount = 50) const;

    /**
     * 특정 이벤트 ID로 Intent 조회
     * @param EventId 조회할 이벤트 ID
     * @param OutEntry 결과 저장
     * @return 찾으면 true
     */
    bool GetIntentEventById(int32 EventId, FHktInsightsIntentEntry& OutEntry) const;

    /**
     * 특정 VM ID로 VM 정보 조회
     * @param VMId 조회할 VM ID
     * @param OutEntry 결과 저장
     * @return 찾으면 true
     */
    bool GetVMById(int32 VMId, FHktInsightsVMEntry& OutEntry) const;

    /**
     * 통계 정보 반환
     */
    FHktInsightsStats GetStats() const;

    // ========== Settings ==========

    /**
     * 최대 히스토리 크기 설정
     * @param Size 최대 저장 개수
     */
    void SetMaxHistorySize(int32 Size);

    /**
     * 현재 최대 히스토리 크기 반환
     */
    int32 GetMaxHistorySize() const { return MaxHistorySize; }

    /**
     * 데이터 수집 활성화/비활성화
     */
    void SetEnabled(bool bInEnabled);

    /**
     * 데이터 수집이 활성화되어 있는지 확인
     */
    bool IsEnabled() const { return bEnabled; }

    /**
     * 모든 데이터 클리어
     */
    void Clear();

    // ========== Delegates ==========

    /** 데이터가 업데이트되었을 때 */
    FOnHktInsightsDataUpdated OnDataUpdated;

    /** Intent 이벤트가 기록되었을 때 */
    FOnHktInsightsIntentRecorded OnIntentRecorded;

    /** VM이 생성되었을 때 */
    FOnHktInsightsVMCreated OnVMCreated;

    /** VM이 완료되었을 때 */
    FOnHktInsightsVMCompleted OnVMCompleted;

private:
    FHktInsightsDataCollector();
    ~FHktInsightsDataCollector() = default;

    // Non-copyable
    FHktInsightsDataCollector(const FHktInsightsDataCollector&) = delete;
    FHktInsightsDataCollector& operator=(const FHktInsightsDataCollector&) = delete;

    /** 스레드 동기화용 락 */
    mutable FCriticalSection DataLock;

    /** Intent 이벤트 히스토리 */
    TArray<FHktInsightsIntentEntry> IntentHistory;

    /** EventId -> Intent Index 맵 (빠른 조회용) */
    TMap<int32, int32> IntentIndexMap;

    /** 활성 VM 맵 (VMId -> Entry) */
    TMap<int32, FHktInsightsVMEntry> ActiveVMMap;

    /** 완료된 VM 히스토리 */
    TArray<FHktInsightsVMEntry> CompletedVMHistory;

    /** 최대 히스토리 크기 */
    int32 MaxHistorySize = 500;

    /** 데이터 수집 활성화 여부 */
    bool bEnabled = true;

    /** 히스토리 크기 제한 적용 */
    void TrimHistory();

    /** 현재 시간 반환 (게임 시작 후 초) */
    double GetCurrentTimestamp() const;
};

// ========== 매크로 헬퍼 ==========

// WITH_HKT_INSIGHTS가 정의되지 않은 경우 기본값 설정
#ifndef WITH_HKT_INSIGHTS
    #if UE_BUILD_SHIPPING
        #define WITH_HKT_INSIGHTS 0
    #else
        #define WITH_HKT_INSIGHTS 1
    #endif
#endif

#if WITH_HKT_INSIGHTS
    // Intent 이벤트 기록 (기본 Pending 상태)
    #define HKT_INSIGHTS_RECORD_INTENT(EventId, EventTag, SubjectId, TargetId, Location) \
        FHktInsightsDataCollector::Get().RecordIntentEvent(EventId, EventTag, SubjectId, TargetId, Location)

    // Intent 이벤트 기록 (상태 지정)
    #define HKT_INSIGHTS_RECORD_INTENT_WITH_STATE(EventId, EventTag, SubjectId, TargetId, Location, State) \
        FHktInsightsDataCollector::Get().RecordIntentEvent(EventId, EventTag, SubjectId, TargetId, Location, State)

    // Intent 이벤트 상태 업데이트
    #define HKT_INSIGHTS_UPDATE_INTENT_STATE(EventId, NewState) \
        FHktInsightsDataCollector::Get().UpdateIntentEventState(EventId, NewState)

    // VM 생성 기록
    #define HKT_INSIGHTS_RECORD_VM_CREATED(VMId, EventId, EventTag, BytecodeSize, SubjectId) \
        FHktInsightsDataCollector::Get().RecordVMCreated(VMId, EventId, EventTag, BytecodeSize, SubjectId)

    // VM Tick 기록
    #define HKT_INSIGHTS_RECORD_VM_TICK(VMId, PC, State, OpName) \
        FHktInsightsDataCollector::Get().RecordVMTick(VMId, PC, State, OpName)

    // VM 완료 기록
    #define HKT_INSIGHTS_RECORD_VM_COMPLETED(VMId, bSuccess) \
        FHktInsightsDataCollector::Get().RecordVMCompleted(VMId, bSuccess)
#else
    #define HKT_INSIGHTS_RECORD_INTENT(EventId, EventTag, SubjectId, TargetId, Location)
    #define HKT_INSIGHTS_RECORD_INTENT_WITH_STATE(EventId, EventTag, SubjectId, TargetId, Location, State)
    #define HKT_INSIGHTS_UPDATE_INTENT_STATE(EventId, NewState)
    #define HKT_INSIGHTS_RECORD_VM_CREATED(VMId, EventId, EventTag, BytecodeSize, SubjectId)
    #define HKT_INSIGHTS_RECORD_VM_TICK(VMId, PC, State, OpName)
    #define HKT_INSIGHTS_RECORD_VM_COMPLETED(VMId, bSuccess)
#endif
