#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "HktCoreTypes.h"
#include "HktCoreInterfaces.h"
#include "HktVMTypes.h"
#include "HktVMRuntime.h"
#include "HktVMStore.h"

// Forward declarations
enum class EVMStatus : uint8;

/**
 * FHktVMProcessor - 3단계 파이프라인으로 VM들을 처리 (Pure C++)
 *
 * Build:   IntentEvent/SystemEvent → VM 생성
 * Execute: 모든 VM yield까지 실행
 * Cleanup: 결과 적용, 완료된 VM 정리
 *
 * "Resolve Now, React Later" 패턴:
 * - Tick(): IntentEvent 처리 (유저 입력)
 * - ProcessSystemEvents(): 지연된 SystemEvent 처리 (충돌 반응 등)
 *
 * UObject/UWorld 참조 없음 - HktCore의 순수성 유지
 */
class HKTCORE_API FHktVMProcessor : public IHktVMProcessorInterface
{
public:
    FHktVMProcessor() = default;
    virtual ~FHktVMProcessor() override;

    void Initialize(IHktStashInterface* InStash);

    // IHktVMProcessorInterface 구현
    virtual void Tick(int32 CurrentFrame, float DeltaSeconds) override;
    virtual void NotifyIntentEvent(const FHktIntentEvent& Event) override;
    virtual void ProcessSystemEvents(
        const TArray<FHktSystemEvent>& Events,
        int32 CurrentFrame,
        float DeltaSeconds) override;

private:
    // Phase 1
    void Build(int32 CurrentFrame);
    void BuildSystemEvents(const TArray<FHktSystemEvent>& Events, int32 CurrentFrame);
    TArray<FHktIntentEvent> PullIntentEvents();
    TOptional<FHktVMHandle> TryCreateVM(const FHktIntentEvent& Event, int32 CurrentFrame);
    TOptional<FHktVMHandle> TryCreateVMForSystemEvent(const FHktSystemEvent& Event, int32 CurrentFrame);

    // Phase 2
    void Execute(float DeltaSeconds);
    EVMStatus ExecuteUntilYield(FHktVMHandle Handle, float DeltaSeconds);

    // Phase 3
    void Cleanup(int32 CurrentFrame);
    void ApplyStoreChanges(FHktVMHandle Handle);
    void FinalizeVM(FHktVMHandle Handle);

private:
    IHktStashInterface* Stash = nullptr;
    
    FHktVMRuntimePool RuntimePool;
    TArray<FHktVMStore> StorePool;
    
    TArray<FHktIntentEvent> PendingEvents;
    TArray<FHktPendingEvent> PendingExternalEvents;
    TArray<FHktVMHandle> PendingVMs;
    TArray<FHktVMHandle> ActiveVMs;
    TArray<FHktVMHandle> CompletedVMs;
    
    class FHktVMInterpreter* Interpreter = nullptr;
};

