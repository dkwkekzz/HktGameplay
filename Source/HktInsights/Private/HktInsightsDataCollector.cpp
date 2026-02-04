// Copyright HKT. All Rights Reserved.

#include "HktInsightsDataCollector.h"
#include "HktInsightsLog.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

FHktInsightsDataCollector& FHktInsightsDataCollector::Get()
{
    static FHktInsightsDataCollector Instance;
    return Instance;
}

FHktInsightsDataCollector::FHktInsightsDataCollector()
{
    IntentHistory.Reserve(MaxHistorySize);
    CompletedVMHistory.Reserve(MaxHistorySize);
}

void FHktInsightsDataCollector::RecordIntentEvent(
    int32 EventId,
    const FGameplayTag& EventTag,
    int32 SubjectId,
    int32 TargetId,
    const FVector& Location,
    EHktInsightsEventState State)
{
    if (!bEnabled)
    {
        return;
    }

    FScopeLock Lock(&DataLock);

    FHktInsightsIntentEntry Entry;
    Entry.EventId = EventId;
    Entry.FrameNumber = GFrameNumber;
    Entry.EventTag = EventTag;
    Entry.SubjectId = SubjectId;
    Entry.TargetId = TargetId;
    Entry.Location = Location;
    Entry.Timestamp = GetCurrentTimestamp();
    Entry.State = State;

    int32 Index = IntentHistory.Add(Entry);
    IntentIndexMap.Add(EventId, Index);

    TrimHistory();

    // 델리게이트 브로드캐스트 (락 외부에서 해야 하지만, 여기서는 간단히 처리)
    OnIntentRecorded.Broadcast(Entry);
    OnDataUpdated.Broadcast();

    UE_LOG(LogHktInsights, Verbose, TEXT("[HktInsights] Intent Recorded: ID=%d, Tag=%s, Subject=%d, State=%s"),
        EventId, *EventTag.ToString(), SubjectId, *Entry.GetStateString());
}

void FHktInsightsDataCollector::UpdateIntentEventState(int32 EventId, EHktInsightsEventState NewState)
{
    if (!bEnabled)
    {
        return;
    }

    FScopeLock Lock(&DataLock);

    if (int32* IndexPtr = IntentIndexMap.Find(EventId))
    {
        if (IntentHistory.IsValidIndex(*IndexPtr))
        {
            IntentHistory[*IndexPtr].State = NewState;

            UE_LOG(LogHktInsights, Verbose, TEXT("[HktInsights] Intent State Updated: ID=%d, NewState=%s"),
                EventId, *IntentHistory[*IndexPtr].GetStateString());

            OnDataUpdated.Broadcast();
        }
    }
}

void FHktInsightsDataCollector::RecordVMCreated(
    int32 VMId,
    int32 SourceEventId,
    const FGameplayTag& SourceEventTag,
    int32 BytecodeSize,
    int32 SubjectId)
{
    if (!bEnabled)
    {
        return;
    }

    FScopeLock Lock(&DataLock);

    FHktInsightsVMEntry Entry;
    Entry.VMId = VMId;
    Entry.SourceEventId = SourceEventId;
    Entry.SourceEventTag = SourceEventTag;
    Entry.BytecodeSize = BytecodeSize;
    Entry.SubjectId = SubjectId;
    Entry.CreationTimestamp = GetCurrentTimestamp();
    Entry.State = EHktInsightsVMState::Running;

    ActiveVMMap.Add(VMId, Entry);

    OnVMCreated.Broadcast(Entry);
    OnDataUpdated.Broadcast();

    UE_LOG(LogHktInsights, Verbose, TEXT("[HktInsights] VM Created: ID=%d, EventId=%d, Tag=%s, BytecodeSize=%d"),
        VMId, SourceEventId, *SourceEventTag.ToString(), BytecodeSize);
}

void FHktInsightsDataCollector::RecordVMTick(
    int32 VMId,
    int32 ProgramCounter,
    EHktInsightsVMState State,
    const FString& CurrentOpcodeName)
{
    if (!bEnabled)
    {
        return;
    }

    FScopeLock Lock(&DataLock);

    if (FHktInsightsVMEntry* Entry = ActiveVMMap.Find(VMId))
    {
        Entry->ProgramCounter = ProgramCounter;
        Entry->State = State;
        Entry->CurrentOpcodeName = CurrentOpcodeName;
        Entry->ElapsedTime = static_cast<float>(GetCurrentTimestamp() - Entry->CreationTimestamp);

        // Tick마다 브로드캐스트하면 부하가 크므로, 델리게이트는 생략하거나 쓰로틀링 적용
        // OnDataUpdated.Broadcast();
    }
}

void FHktInsightsDataCollector::RecordVMCompleted(int32 VMId, bool bSuccess)
{
    if (!bEnabled)
    {
        return;
    }

    FScopeLock Lock(&DataLock);

    if (FHktInsightsVMEntry* Entry = ActiveVMMap.Find(VMId))
    {
        Entry->CompletionTimestamp = GetCurrentTimestamp();
        Entry->ElapsedTime = static_cast<float>(Entry->CompletionTimestamp - Entry->CreationTimestamp);
        Entry->State = bSuccess ? EHktInsightsVMState::Completed : EHktInsightsVMState::Error;

        // 완료 히스토리에 추가
        CompletedVMHistory.Add(*Entry);

        // 히스토리 크기 제한
        if (CompletedVMHistory.Num() > MaxHistorySize)
        {
            CompletedVMHistory.RemoveAt(0, CompletedVMHistory.Num() - MaxHistorySize);
        }

        FHktInsightsVMEntry CompletedEntry = *Entry;

        // Active에서 제거
        ActiveVMMap.Remove(VMId);

        OnVMCompleted.Broadcast(CompletedEntry);
        OnDataUpdated.Broadcast();

        UE_LOG(LogHktInsights, Verbose, TEXT("[HktInsights] VM Completed: ID=%d, Success=%d, ElapsedTime=%.3fs"),
            VMId, bSuccess, CompletedEntry.ElapsedTime);
    }
}

TArray<FHktInsightsIntentEntry> FHktInsightsDataCollector::GetRecentIntentEvents(int32 MaxCount) const
{
    FScopeLock Lock(&DataLock);

    TArray<FHktInsightsIntentEntry> Result;
    int32 StartIndex = FMath::Max(0, IntentHistory.Num() - MaxCount);
    
    for (int32 i = IntentHistory.Num() - 1; i >= StartIndex; --i)
    {
        Result.Add(IntentHistory[i]);
    }

    return Result;
}

TArray<FHktInsightsIntentEntry> FHktInsightsDataCollector::GetIntentEventsByState(EHktInsightsEventState State, int32 MaxCount) const
{
    FScopeLock Lock(&DataLock);

    TArray<FHktInsightsIntentEntry> Result;
    
    for (int32 i = IntentHistory.Num() - 1; i >= 0 && Result.Num() < MaxCount; --i)
    {
        if (IntentHistory[i].State == State)
        {
            Result.Add(IntentHistory[i]);
        }
    }

    return Result;
}

TArray<FHktInsightsVMEntry> FHktInsightsDataCollector::GetActiveVMs() const
{
    FScopeLock Lock(&DataLock);

    TArray<FHktInsightsVMEntry> Result;
    ActiveVMMap.GenerateValueArray(Result);

    // VM ID 순서로 정렬
    Result.Sort([](const FHktInsightsVMEntry& A, const FHktInsightsVMEntry& B)
    {
        return A.VMId > B.VMId; // 최신 것이 먼저
    });

    return Result;
}

TArray<FHktInsightsVMEntry> FHktInsightsDataCollector::GetRecentCompletedVMs(int32 MaxCount) const
{
    FScopeLock Lock(&DataLock);

    TArray<FHktInsightsVMEntry> Result;
    int32 StartIndex = FMath::Max(0, CompletedVMHistory.Num() - MaxCount);
    
    for (int32 i = CompletedVMHistory.Num() - 1; i >= StartIndex; --i)
    {
        Result.Add(CompletedVMHistory[i]);
    }

    return Result;
}

bool FHktInsightsDataCollector::GetIntentEventById(int32 EventId, FHktInsightsIntentEntry& OutEntry) const
{
    FScopeLock Lock(&DataLock);

    if (const int32* IndexPtr = IntentIndexMap.Find(EventId))
    {
        if (IntentHistory.IsValidIndex(*IndexPtr))
        {
            OutEntry = IntentHistory[*IndexPtr];
            return true;
        }
    }

    return false;
}

bool FHktInsightsDataCollector::GetVMById(int32 VMId, FHktInsightsVMEntry& OutEntry) const
{
    FScopeLock Lock(&DataLock);

    // 활성 VM에서 먼저 찾기
    if (const FHktInsightsVMEntry* Entry = ActiveVMMap.Find(VMId))
    {
        OutEntry = *Entry;
        return true;
    }

    // 완료된 VM에서 찾기
    for (const FHktInsightsVMEntry& Entry : CompletedVMHistory)
    {
        if (Entry.VMId == VMId)
        {
            OutEntry = Entry;
            return true;
        }
    }

    return false;
}

FHktInsightsStats FHktInsightsDataCollector::GetStats() const
{
    FScopeLock Lock(&DataLock);

    FHktInsightsStats Stats;
    Stats.TotalEventCount = IntentHistory.Num();
    Stats.ActiveVMCount = ActiveVMMap.Num();
    Stats.CompletedVMCount = CompletedVMHistory.Num();

    // 상태별 카운트
    for (const FHktInsightsIntentEntry& Entry : IntentHistory)
    {
        if (Entry.State == EHktInsightsEventState::Pending || 
            Entry.State == EHktInsightsEventState::Processing)
        {
            Stats.PendingEventCount++;
        }
        else if (Entry.State == EHktInsightsEventState::Failed)
        {
            Stats.FailedEventCount++;
        }
    }

    // 평균 VM 실행 시간 계산
    if (CompletedVMHistory.Num() > 0)
    {
        float TotalTime = 0.0f;
        for (const FHktInsightsVMEntry& Entry : CompletedVMHistory)
        {
            TotalTime += Entry.ElapsedTime;
        }
        Stats.AverageVMExecutionTime = TotalTime / CompletedVMHistory.Num();
    }

    return Stats;
}

void FHktInsightsDataCollector::SetMaxHistorySize(int32 Size)
{
    FScopeLock Lock(&DataLock);
    MaxHistorySize = FMath::Max(10, Size);
    TrimHistory();
}

void FHktInsightsDataCollector::SetEnabled(bool bInEnabled)
{
    bEnabled = bInEnabled;
    
    UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] Data collection %s"), 
        bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void FHktInsightsDataCollector::Clear()
{
    FScopeLock Lock(&DataLock);

    IntentHistory.Empty();
    IntentIndexMap.Empty();
    ActiveVMMap.Empty();
    CompletedVMHistory.Empty();

    UE_LOG(LogHktInsights, Log, TEXT("[HktInsights] All data cleared"));

    OnDataUpdated.Broadcast();
}

void FHktInsightsDataCollector::TrimHistory()
{
    // Intent 히스토리 제한
    if (IntentHistory.Num() > MaxHistorySize)
    {
        int32 RemoveCount = IntentHistory.Num() - MaxHistorySize;
        
        // 인덱스 맵 업데이트
        for (int32 i = 0; i < RemoveCount; ++i)
        {
            IntentIndexMap.Remove(IntentHistory[i].EventId);
        }
        
        IntentHistory.RemoveAt(0, RemoveCount);

        // 남은 항목들의 인덱스 재계산
        IntentIndexMap.Empty();
        for (int32 i = 0; i < IntentHistory.Num(); ++i)
        {
            IntentIndexMap.Add(IntentHistory[i].EventId, i);
        }
    }

    // 완료된 VM 히스토리 제한
    if (CompletedVMHistory.Num() > MaxHistorySize)
    {
        CompletedVMHistory.RemoveAt(0, CompletedVMHistory.Num() - MaxHistorySize);
    }
}

double FHktInsightsDataCollector::GetCurrentTimestamp() const
{
    // 게임 시작 후 경과 시간 (초)
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        const FWorldContext& Context = GEngine->GetWorldContexts()[0];
        if (Context.World())
        {
            return Context.World()->GetTimeSeconds();
        }
    }
    
    return FPlatformTime::Seconds();
}
