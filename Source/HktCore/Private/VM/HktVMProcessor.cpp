#include "HktVMProcessor.h"
#include "HktCoreInterfaces.h"
#include "HktVMInterpreter.h"
#include "HktVMStore.h"
#include "HktVMProgram.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#endif

FHktVMProcessor::~FHktVMProcessor()
{
    if (Interpreter)
    {
        delete Interpreter;
        Interpreter = nullptr;
    }
}

void FHktVMProcessor::Initialize(IHktStashInterface* InStash)
{
    Stash = InStash;
    
    Interpreter = new FHktVMInterpreter();
    Interpreter->Initialize(Stash);

    // RuntimePool은 인라인 멤버이므로 Reset으로 초기화
    RuntimePool.Reset();

    // Store 풀 초기화
    StorePool.SetNum(256);
    for (FHktVMStore& Store : StorePool)
    {
        Store.Stash = Stash;
    }
}

void FHktVMProcessor::Tick(int32 CurrentFrame, float DeltaSeconds)
{
    Build(CurrentFrame);
    Execute(DeltaSeconds);
    Cleanup(CurrentFrame);
}

void FHktVMProcessor::NotifyIntentEvent(const FHktIntentEvent& Event)
{
    PendingEvents.Add(Event);

    // HktInsights: Intent 이벤트 기록
    HKT_INSIGHTS_RECORD_INTENT(
        Event.EventId,
        Event.EventTag,
        static_cast<int32>(Event.SourceEntity),
        static_cast<int32>(Event.TargetEntity),
        Event.Location
    );
}

// ============================================================================
// Event Notifications
// ============================================================================

void FHktVMProcessor::NotifyCollision(EntityId WatchedEntity, EntityId HitEntity)
{
    RuntimePool.ForEachActive([&](FHktVMHandle Handle, FHktVMRuntime& Runtime)
    {
        if (Runtime.Status == EVMStatus::WaitingEvent &&
            Runtime.EventWait.Type == EWaitEventType::Collision &&
            Runtime.EventWait.WatchedEntity == WatchedEntity)
        {
            Interpreter->NotifyCollision(Runtime, HitEntity);
        }
    });
}

void FHktVMProcessor::NotifyAnimEnd(EntityId Entity)
{
    RuntimePool.ForEachActive([&](FHktVMHandle Handle, FHktVMRuntime& Runtime)
    {
        if (Runtime.Status == EVMStatus::WaitingEvent &&
            Runtime.EventWait.Type == EWaitEventType::AnimationEnd &&
            Runtime.EventWait.WatchedEntity == Entity)
        {
            Interpreter->NotifyAnimEnd(Runtime);
        }
    });
}

void FHktVMProcessor::NotifyMoveEnd(EntityId Entity)
{
    RuntimePool.ForEachActive([&](FHktVMHandle Handle, FHktVMRuntime& Runtime)
    {
        if (Runtime.Status == EVMStatus::WaitingEvent &&
            Runtime.EventWait.Type == EWaitEventType::MovementEnd &&
            Runtime.EventWait.WatchedEntity == Entity)
        {
            Interpreter->NotifyMoveEnd(Runtime);
        }
    });
}

// ============================================================================
// Phase 1: Build
// ============================================================================

void FHktVMProcessor::Build(int32 CurrentFrame)
{
    TArray<FHktIntentEvent> Events = PullIntentEvents();
    for (const FHktIntentEvent& Event : Events)
    {
        // VM 생성
        TOptional<FHktVMHandle> Handle = TryCreateVM(Event, CurrentFrame);
        if (Handle.IsSet())
        {
            PendingVMs.Add(Handle.GetValue());
        }
    }
    
    ActiveVMs.Append(PendingVMs);
    PendingVMs.Reset();
    
    // Yielded VM 재활성화
    RuntimePool.ForEachActive([](FHktVMHandle Handle, FHktVMRuntime& Runtime)
    {
        if (Runtime.Status == EVMStatus::Yielded)
        {
            if (Runtime.WaitFrames <= 0)
            {
                Runtime.Status = EVMStatus::Ready;
            }
            else
            {
                Runtime.WaitFrames--;
            }
        }
    });
}

TArray<FHktIntentEvent> FHktVMProcessor::PullIntentEvents()
{
    TArray<FHktIntentEvent> Result = MoveTemp(PendingEvents);
    PendingEvents.Reset();
    return Result;
}

TOptional<FHktVMHandle> FHktVMProcessor::TryCreateVM(const FHktIntentEvent& Event, int32 CurrentFrame)
{
    if (!Stash || !Stash->IsValidEntity(Event.SourceEntity))
    {
        UE_LOG(LogTemp, Warning, TEXT("VM creation failed: SourceEntity %u not valid"), (int32)Event.SourceEntity);
        return {};
    }
    
    const FHktVMProgram* Program = FHktVMProgramRegistry::Get().FindProgram(Event.EventTag);
    if (!Program)
    {
        UE_LOG(LogTemp, Warning, TEXT("VM creation failed: No program for %s"), *Event.EventTag.ToString());
        return {};
    }
    
    FHktVMHandle Handle = RuntimePool.Allocate();
    if (!Handle.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("VM creation failed: Pool exhausted"));
        return {};
    }
    
    FHktVMRuntime* Runtime = RuntimePool.Get(Handle);
    check(Runtime);
    
    // Store 할당
    FHktVMStore& Store = StorePool[Handle.Index];
    Store.Stash = Stash;
    Store.SourceEntity = Event.SourceEntity;
    Store.TargetEntity = Event.TargetEntity;
    Store.ClearPendingWrites();
    Store.LocalCache.Reset();
    
    // Runtime 초기화
    Runtime->Program = Program;
    Runtime->Store = &Store;
    Runtime->PC = 0;
    Runtime->Status = EVMStatus::Ready;
    Runtime->CreationFrame = CurrentFrame;
    Runtime->WaitFrames = 0;
    Runtime->EventWait.Reset();
    Runtime->SpatialQuery.Reset();
    FMemory::Memzero(Runtime->Registers, sizeof(Runtime->Registers));

#if !UE_BUILD_SHIPPING
    Runtime->SourceEventId = Event.EventId;  // 디버그용 EventId 저장
#endif
    
    Runtime->SetRegEntity(Reg::Self, Event.SourceEntity);
    Runtime->SetRegEntity(Reg::Target, Event.TargetEntity);
    
    // Payload에서 파라미터 추출 (int32 배열로 해석)
    const int32* PayloadParams = reinterpret_cast<const int32*>(Event.Payload.GetData());
    const int32 NumParams = FMath::Min((int32)(Event.Payload.Num() / sizeof(int32)), 4);
    for (int32 i = 0; i < NumParams; ++i)
    {
        Store.Write(PropertyId::Param0 + i, PayloadParams[i]);
    }
    
    // 타겟 위치 설정 (Event.Location 사용)
    Store.Write(PropertyId::TargetPosX, FMath::RoundToInt(Event.Location.X));
    Store.Write(PropertyId::TargetPosY, FMath::RoundToInt(Event.Location.Y));
    Store.Write(PropertyId::TargetPosZ, FMath::RoundToInt(Event.Location.Z));
    
    UE_LOG(LogTemp, Log, TEXT("VM created: %s for Entity %u"), *Event.EventTag.ToString(), (int32)Event.SourceEntity);
    
    // HktInsights: VM 생성 기록
    HKT_INSIGHTS_RECORD_VM_CREATED(
        Handle.Index,          // VMId로 Handle.Index 사용
        Event.EventId,
        Event.EventTag,
        Program->CodeSize(),
        static_cast<int32>(Event.SourceEntity)
    );

    // HktInsights: Intent 이벤트 상태를 Processing으로 변경
    HKT_INSIGHTS_UPDATE_INTENT_STATE(Event.EventId, EHktInsightsEventState::Processing);

    return Handle;
}

// ============================================================================
// Phase 2: Execute
// ============================================================================

void FHktVMProcessor::Execute(float DeltaSeconds)
{
    RuntimePool.ForEachActive([&](FHktVMHandle Handle, FHktVMRuntime& Runtime)
    {
        if (Runtime.Status == EVMStatus::WaitingEvent &&
            Runtime.EventWait.Type == EWaitEventType::Timer)
        {
            Interpreter->UpdateTimer(Runtime, DeltaSeconds);
        }
    });
    
    for (int32 i = ActiveVMs.Num() - 1; i >= 0; --i)
    {
        FHktVMHandle Handle = ActiveVMs[i];
        EVMStatus Status = ExecuteUntilYield(Handle, DeltaSeconds);
        
        if (Status == EVMStatus::Completed || Status == EVMStatus::Failed)
        {
            CompletedVMs.Add(Handle);
            ActiveVMs.RemoveAtSwap(i);
        }
    }
}

EVMStatus FHktVMProcessor::ExecuteUntilYield(FHktVMHandle Handle, float DeltaSeconds)
{
    FHktVMRuntime* Runtime = RuntimePool.Get(Handle);
    if (!Runtime) return EVMStatus::Failed;
    
    if (!Runtime->IsRunnable())
        return Runtime->Status;
    
    Runtime->Status = EVMStatus::Running;
    EVMStatus Result = Interpreter->Execute(*Runtime);
    Runtime->Status = Result;

    // HktInsights: VM Tick 기록
#if WITH_HKT_INSIGHTS
    {
        EHktInsightsVMState VMState;
        switch (Result)
        {
        case EVMStatus::Running:
        case EVMStatus::Ready:
            VMState = EHktInsightsVMState::Running;
            break;
        case EVMStatus::Yielded:
        case EVMStatus::WaitingEvent:
            VMState = EHktInsightsVMState::Blocked;
            break;
        case EVMStatus::Completed:
            VMState = EHktInsightsVMState::Completed;
            break;
        case EVMStatus::Failed:
            VMState = EHktInsightsVMState::Error;
            break;
        default:
            VMState = EHktInsightsVMState::Running;
            break;
        }

        FString OpName;
        if (Runtime->Program && Runtime->PC >= 0 && Runtime->PC < Runtime->Program->CodeSize())
        {
            const FInstruction& Inst = Runtime->Program->Code[Runtime->PC];
            OpName = FString::Printf(TEXT("OP_%02X"), static_cast<uint8>(Inst.GetOpCode()));
        }

        HKT_INSIGHTS_RECORD_VM_TICK(Handle.Index, Runtime->PC, VMState, OpName);
    }
#endif
    
    return Result;
}

// ============================================================================
// Phase 3: Cleanup
// ============================================================================

void FHktVMProcessor::Cleanup(int32 CurrentFrame)
{
    for (FHktVMHandle Handle : CompletedVMs)
    {
        ApplyStoreChanges(Handle);
        FinalizeVM(Handle);
    }
    CompletedVMs.Reset();
}

void FHktVMProcessor::ApplyStoreChanges(FHktVMHandle Handle)
{
    FHktVMRuntime* Runtime = RuntimePool.Get(Handle);
    if (!Runtime || !Runtime->Store || !Stash) return;
    
    for (const FHktVMStore::FPendingWrite& W : Runtime->Store->PendingWrites)
    {
        Stash->SetProperty(W.Entity, W.PropertyId, W.Value);
    }
    Runtime->Store->ClearPendingWrites();
}

void FHktVMProcessor::FinalizeVM(FHktVMHandle Handle)
{
    FHktVMRuntime* Runtime = RuntimePool.Get(Handle);
    if (Runtime)
    {
        UE_LOG(LogTemp, Log, TEXT("VM finalized: %s"), 
            Runtime->Program ? *Runtime->Program->Tag.ToString() : TEXT("unknown"));

        // HktInsights: VM 완료 기록
        bool bSuccess = (Runtime->Status == EVMStatus::Completed);
        HKT_INSIGHTS_RECORD_VM_COMPLETED(Handle.Index, bSuccess);

        // HktInsights: Intent 이벤트 상태를 Completed/Failed로 업데이트
#if !UE_BUILD_SHIPPING
        if (Runtime->SourceEventId != 0)
        {
            EHktInsightsEventState FinalState = bSuccess 
                ? EHktInsightsEventState::Completed 
                : EHktInsightsEventState::Failed;
            HKT_INSIGHTS_UPDATE_INTENT_STATE(Runtime->SourceEventId, FinalState);
        }
#endif
        
        if (Runtime->Store)
        {
            Runtime->Store->Reset();
        }
    }
    RuntimePool.Free(Handle);
}