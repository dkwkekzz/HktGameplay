#pragma once

#include "CoreMinimal.h"
#include "HktVMTypes.h"

// Forward declarations
struct FHktVMProgram;
struct FHktVMStore;

/**
 * FSpatialQueryResult - 공간 검색 결과 저장
 */
struct FSpatialQueryResult
{
    TArray<EntityId> Entities;
    int32 CurrentIndex = 0;
    
    void Reset()
    {
        Entities.Reset();
        CurrentIndex = 0;
    }
    
    bool HasNext() const
    {
        return CurrentIndex < Entities.Num();
    }
    
    EntityId Next()
    {
        if (HasNext())
        {
            return Entities[CurrentIndex++];
        }
        return InvalidEntityId;
    }
};

/**
 * FEventWaitState - 이벤트 대기 상태
 */
struct FEventWaitState
{
    EWaitEventType Type = EWaitEventType::None;
    EntityId WatchedEntity = InvalidEntityId;
    float RemainingTime = 0.0f;  // Timer용
    
    void Reset()
    {
        Type = EWaitEventType::None;
        WatchedEntity = InvalidEntityId;
        RemainingTime = 0.0f;
    }
};

/**
 * FHktVMRuntime - 단일 VM의 실행 상태
 */
struct HKTCORE_API FHktVMRuntime
{
    /** 실행 중인 프로그램 (공유, 불변) */
    const FHktVMProgram* Program = nullptr;
    
    /** 로컬 데이터 스토어 */
    struct FHktVMStore* Store = nullptr;
    
    /** 프로그램 카운터 */
    int32 PC = 0;
    
    /** 범용 레지스터 (R0-R15) */
    int32 Registers[MaxRegisters] = {0};
    
    /** 현재 상태 */
    EVMStatus Status = EVMStatus::Ready;
    
    /** 생성 프레임 */
    int32 CreationFrame = 0;
    
    /** Yield 후 대기 프레임 수 */
    int32 WaitFrames = 0;
    
    /** 이벤트 대기 상태 */
    FEventWaitState EventWait;
    
    /** 공간 검색 결과 (FindInRadius) */
    FSpatialQueryResult SpatialQuery;

#if !UE_BUILD_SHIPPING
    /** 디버그용: 이 VM을 생성한 이벤트 ID (HktInsights 추적용) */
    int32 SourceEventId = 0;
#endif
    
    // ========== 레지스터 헬퍼 ==========
    
    int32 GetReg(RegisterIndex Idx) const 
    { 
        check(Idx < MaxRegisters);
        return Registers[Idx]; 
    }
    
    void SetReg(RegisterIndex Idx, int32 Value) 
    { 
        check(Idx < MaxRegisters);
        Registers[Idx] = Value; 
    }
    
    float GetRegFloat(RegisterIndex Idx) const
    {
        return *reinterpret_cast<const float*>(&Registers[Idx]);
    }
    
    void SetRegFloat(RegisterIndex Idx, float Value)
    {
        Registers[Idx] = *reinterpret_cast<const int32*>(&Value);
    }
    
    /** 엔티티 ID로 해석 */
    EntityId GetRegEntity(RegisterIndex Idx) const
    {
        return static_cast<EntityId>(Registers[Idx]);
    }
    
    void SetRegEntity(RegisterIndex Idx, EntityId Entity)
    {
        Registers[Idx] = static_cast<int32>(Entity);
    }
    
    // ========== 상태 검사 ==========
    
    bool IsRunnable() const 
    { 
        return Status == EVMStatus::Ready || Status == EVMStatus::Running; 
    }
    
    bool IsYielded() const { return Status == EVMStatus::Yielded; }
    bool IsWaitingEvent() const { return Status == EVMStatus::WaitingEvent; }
    bool IsCompleted() const { return Status == EVMStatus::Completed; }
    bool IsFailed() const { return Status == EVMStatus::Failed; }
    bool IsTerminated() const { return IsCompleted() || IsFailed(); }
    
    // ========== 디버그 ==========
    
    FString GetDebugString() const;
};

// ============================================================================
// FHktVMRuntimePool - SOA 레이아웃의 런타임 풀
// ============================================================================

class HKTCORE_API FHktVMRuntimePool
{
public:
    FHktVMRuntimePool();
    
    FHktVMHandle Allocate();
    void Free(FHktVMHandle Handle);
    
    FHktVMRuntime* Get(FHktVMHandle Handle);
    const FHktVMRuntime* Get(FHktVMHandle Handle) const;
    
    bool IsValid(FHktVMHandle Handle) const;
    
    template<typename Func>
    void ForEachActive(Func&& Callback);
    
    int32 CountByStatus(EVMStatus Status) const;
    void Reset();

private:
    static constexpr int32 MaxVMs = 256;
    
    TArray<EVMStatus> Statuses;
    TArray<int32> PCs;
    TArray<int32> WaitFrames;
    TArray<uint8> Generations;
    TArray<FHktVMRuntime> Runtimes;
    TArray<uint32> FreeSlots;
};

// ============================================================================
// Template 구현
// ============================================================================

template<typename Func>
void FHktVMRuntimePool::ForEachActive(Func&& Callback)
{
    for (int32 i = 0; i < Runtimes.Num(); ++i)
    {
        EVMStatus S = Statuses[i];
        if (S != EVMStatus::Completed && S != EVMStatus::Failed)
        {
            FHktVMHandle Handle;
            Handle.Index = i;
            Handle.Generation = Generations[i];
            Callback(Handle, Runtimes[i]);
        }
    }
}