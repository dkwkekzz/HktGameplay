** WorldState soa화, Publish RenderData 정책 
문제의 핵심:
SOA(Structure of Arrays) 전환 필요: 데이터를 컬럼(Column) 형태의 배열로 쪼개야 함.
로컬 데이터(Local Store) 처리 난제: VM이 실행되며 생성한 Local Store(임시 변경 사항)를 메인 SOA 데이터와 어떻게 효율적으로 합쳐서(Merge) 외부(렌더러)에 제공할 것인가?
이를 해결하기 위해 **"청크 기반 SOA(Chunk-based SOA)"**와 "델타 레이어(Delta Layer)" 패턴을 제안합니다.

해결 아키텍처: SOA State + Delta Buffer
이 구조의 핵심은 **"읽기(Read)는 SOA에서, 쓰기(Write)는 Sparse Buffer에, 병합(Merge)은 프레임 끝에 일괄 처리"**하는 것입니다.

1. 데이터 구조 재설계 (HktCoreTypes.h 수정)
기존의 TArray<int8> Properties를 제거하고, FHktWorldState 자체가 컴포넌트별 배열을 관리하도록 변경합니다.

2. 로컬 데이터 처리 (Local Area Problem) 해결책
VM 실행 중 발생하는 변경 사항(Local Store)을 즉시 SOA에 반영하면 안 됩니다(결정론 및 병렬 처리 위반). 대신 **"Pending Writes Buffer"**를 효율적으로 구성하여, 렌더링 시점에 **오버레이(Overlay)**하거나 **커밋(Commit)**합니다.

정리하자면:

WorldState (The Truth): 시뮬레이션 로직이 완전히 끝난 후, 모든 클라이언트가 동기화되어야 하는 확정된 과거/현재입니다. VM이 실행 중일 때(Running/Yielded) 계산하고 있는 중간값(LocalStore)을 여기에 섣불리 반영하면, 다른 클라이언트와 상태가 달라져 **Desync(동기화 오류)**가 발생합니다.

LocalStore (The Potential Future): 해당 기기(로컬 클라이언트)에서 실행 중인 VM이 계산한 **"가장 최신 값"**입니다. 아직 확정되지 않았지만, 유저 눈에는 이게 보여야 합니다 (예: 내 캐릭터가 이동 키를 눌러서 움직이기 시작함).

RenderState (Visual View): 따라서 렌더링 시스템은 WorldState(기반) 위에 LocalStore(현재 실행 중인 VM의 변경사항)를 덮어씌운(Overlay) 결과를 가져가야 합니다.

*** HktCore 핵심 클래스 명세 (C++ Header Specifications) ***
SOA(Structure of Arrays) 패턴을 적용하여 캐시 효율을 확보하고, 렌더링 시스템이 필요한 데이터 컬럼(Column)에만 고속으로 접근할 수 있도록 설계합니다.

1. HKTCore/Public/HktCoreTypes.h

SOA 기반 월드 상태 정의입니다.

// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

// ============================================================================
// Basic Types & IDs
// ============================================================================

using FHktEntityId = int32;
constexpr FHktEntityId InvalidEntityId = -1;

/** 범용 게임플레이 이벤트 */
struct HKTCORE_API FHktEvent
{
    FGameplayTag EventTag;
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    FVector Location = FVector::ZeroVector;
    int32 Param0 = 0;
    int32 Param1 = 0;
};

/** 물리 충돌 이벤트 */
struct HKTCORE_API FHktPhysicsEvent
{
    FHktEntityId EntityA = InvalidEntityId;
    FHktEntityId EntityB = InvalidEntityId;
    FVector ContactPoint = FVector::ZeroVector;
};

/** 프레임 단위 시뮬레이션 입력 */
struct HKTCORE_API FHktSimulationEvent
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    float DeltaSeconds = 0.0f;
    TArray<int64> RemovedOwnerIds;
    TArray<FHktEvent> Events;
};

// ============================================================================
// SOA (Structure of Arrays) World State
// ============================================================================

/** * 단일 프로퍼티에 대한 데이터 컬럼 
 * 예: Health 컬럼, Position 컬럼 등. 
 * 렌더러는 필요한 컬럼 배열만 가져다 쓰므로 캐시 효율이 극대화됩니다.
 */
struct HKTCORE_API FHktDataColumn
{
    int32 PropertyId = -1;
    TArray<int32> IntData;   
    TArray<float> FloatData;

    void Resize(int32 Size)
    {
        IntData.SetNum(Size);
        FloatData.SetNum(Size);
    }
};

struct HKTCORE_API FHktWorldState
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    
    // Entity ID <-> Index 매핑
    TArray<int32> EntityToIndex; 
    TArray<FHktEntityId> IndexToEntity;
    TArray<int32> FreeIndices;

    // Component Data (Columns)
    TMap<int32, FHktDataColumn> Columns;
    TArray<FGameplayTagContainer> TagColumn;

    // ... (Helper 함수 동일) ...
    
    // 컬럼 접근 헬퍼 (렌더링 최적화용)
    const FHktDataColumn* GetColumn(int32 PropertyId) const
    {
        return Columns.Find(PropertyId);
    }

    FHktEntityId CreateEntity();
    void RemoveEntity(FHktEntityId Id);
    void CopyFrom(const FHktWorldState& Other);
};

/** 렌더링용 상태 */
struct HKTCORE_API FHktRenderState
{
    int64 FrameNumber = 0;
    
    // 렌더링 스레드는 완성된 WorldState의 컬럼 중 
    // 화면 표시에 필요한 것만 복사하거나 참조해서 사용합니다.
    TArray<FVector> RenderPositions; 
    TArray<FRotator> RenderRotations;
    TArray<FHktEntityId> RenderEntityIds;
};


2. HKTCore/Public/HktVMTypes.h

VM 런타임, 핸들, 상태 정의입니다.
Local Store가 SOA 구조에 맞춰 변경 사항을 효율적으로 기록하도록 개선되었습니다.

// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"

// ============================================================================
// VM Definitions
// ============================================================================

enum class EVMStatus : uint8
{
    Ready, Running, Yielded, WaitingEvent, Completed, Failed
};

struct FHktVMProgram
{
    FGameplayTag Tag;
    TArray<uint8> ByteCode;
};

/** * VM 로컬 변경 사항 버퍼 
 * - SOA 업데이트 효율을 위해 PropertyID 별로 변경 사항을 묶음(Batching)
 */
struct FHktVMStore
{
    struct FIntChange { FHktEntityId Entity; int32 Value; };
    struct FFloatChange { FHktEntityId Entity; float Value; };

    // Key: PropertyId
    TMap<int32, TArray<FIntChange>> PendingIntWrites;
    TMap<int32, TArray<FFloatChange>> PendingFloatWrites;

    void WriteInt(FHktEntityId Entity, int32 PropId, int32 Val)
    {
        PendingIntWrites.FindOrAdd(PropId).Add({Entity, Val});
    }
    
    void WriteFloat(FHktEntityId Entity, int32 PropId, float Val)
    {
        PendingFloatWrites.FindOrAdd(PropId).Add({Entity, Val});
    }
    
    void Clear() 
    { 
        PendingIntWrites.Reset(); 
        PendingFloatWrites.Reset(); 
    }
};

// ============================================================================
// Runtime Context & Pool
// ============================================================================

struct HKTCORE_API FHktVMRuntime
{
    static constexpr int32 MaxRegisters = 16;

    const FHktVMProgram* Program = nullptr;
    FHktVMStore Store; 
    
    int32 PC = 0;
    int32 Registers[MaxRegisters] = {0};
    EVMStatus Status = EVMStatus::Ready;
    
    int32 CreationFrame = 0;
    int32 WaitFrames = 0;
    FHktEntityId SelfEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    
    bool IsRunnable() const { return Status == EVMStatus::Ready || Status == EVMStatus::Running; }
    
    void SetRegFloat(int32 Idx, float Val) { Registers[Idx] = *reinterpret_cast<int32*>(&Val); }
    float GetRegFloat(int32 Idx) const { return *reinterpret_cast<const float*>(&Registers[Idx]); }
};

struct FHktVMHandle
{
    uint32 Index : 24;
    uint32 Generation : 8;
    bool IsValid() const { return Index != 0xFFFFFF; }
    static FHktVMHandle Invalid() { return {0xFFFFFF, 0}; }
    bool operator==(const FHktVMHandle& Other) const { return Index == Other.Index && Generation == Other.Generation; }
};

class HKTCORE_API FHktVMRuntimePool
{
public:
    FHktVMRuntimePool();
    FHktVMHandle Allocate();
    void Free(FHktVMHandle Handle);
    FHktVMRuntime* Get(FHktVMHandle Handle);
    void Reset();
    
    template<typename Func>
    void ForEachActive(Func&& Callback);

private:
    static constexpr int32 MaxVMs = 1024;
    TArray<FHktVMRuntime> Runtimes;
    TArray<uint8> Generations;
    TArray<uint32> FreeSlots;
};


3. HKTCore/Public/HktSimulationSystems.h

각 단계별 로직 처리기입니다.
ApplyStoreSystem이 SOA 데이터를 효율적으로 업데이트하는 방식이 중요합니다.

// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktVMTypes.h"

// Forward Declarations
class FHktVMInterpreter;

struct HKTCORE_API FHktEntityArrangeSystem
{
    void Process(FHktWorldState& WorldState, const TArray<int64>& RemovedOwnerIds);
};

struct HKTCORE_API FHktVMBuildSystem
{
    void Process(
        const TArray<FHktEvent>& Events, 
        int32 CurrentFrame, 
        FHktVMRuntimePool& Pool, 
        TArray<FHktVMHandle>& OutActiveVMs
    );
};

struct HKTCORE_API FHktVMProcessSystem
{
    FHktVMInterpreter* Interpreter = nullptr;

    void Process(
        TArray<FHktVMHandle>& ActiveVMs, 
        TArray<FHktVMHandle>& OutCompletedVMs,
        FHktVMRuntimePool& Pool,
        float DeltaSeconds
    );
};

struct HKTCORE_API FHktPhysicsSystem
{
    static constexpr float CellSize = 1000.0f;
    
    struct FCellCoord
    {
        int32 X, Y;
        bool operator==(const FCellCoord& Other) const { return X == Other.X && Y == Other.Y; }
        friend uint32 GetTypeHash(const FCellCoord& C) { return HashCombine(C.X, C.Y); }
    };

    TMap<FCellCoord, TArray<FHktEntityId>> GridMap;

    void RebuildGrid(const FHktWorldState& WorldState);
    void Process(FHktWorldState& WorldState, TArray<FHktPhysicsEvent>& OutPhysicsEvents);
};

/** * [중요] SOA Apply System 
 * VM 실행 결과(Store)를 WorldState의 각 Column에 '즉시' 병합합니다.
 * 이 단계가 끝나면 WorldState는 최신 상태가 되며, 렌더러는 이 데이터를 직접 사용합니다.
 * 최적화 포인트: Column별로 모아서(Batch) 쓰기를 수행하여 Cache Miss를 줄일 것.
 */
struct HKTCORE_API FHktApplyStoreSystem
{
    void Process(
        FHktWorldState& WorldState, 
        const TArray<FHktVMHandle>& DirtyVMs,
        FHktVMRuntimePool& Pool
    );
};

struct HKTCORE_API FHktVMCleanupSystem
{
    void Process(TArray<FHktVMHandle>& CompletedVMs, FHktVMRuntimePool& Pool);
};

/** * [중요] Publish System
 * - WorldState(SOA)를 읽어 렌더링에 최적화된 형태로 변환/복사합니다.
 * - UI나 렌더러는 이 RenderState를 참조하므로 Local Data 고민 없이 완성된 데이터만 봄
 */
struct HKTCORE_API FHktPublishRenderSystem
{
    void Process(const FHktWorldState& WorldState, FHktRenderState& OutRenderState);
};


4. HKTCore/Public/HktSimulationWorld.h

전체 파이프라인 조율 클래스입니다.

// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktVMTypes.h"
#include "HktSimulationSystems.h"

/**
 * FHktSimulationWorld
 * - SOA 기반의 State 관리 및 파이프라인 실행
 */
class HKTCORE_API FHktSimulationWorld
{
public:
    FHktSimulationWorld();
    ~FHktSimulationWorld();

    void ProcessBatch(const FHktSimulationEvent& Event);
    void RestoreState(const FHktWorldState& InState);
    void GetStateSnapshot(FHktWorldState& OutState) const;
    void PublishRenderState(FHktRenderState& OutState);

private:
    // --- Data (SOA) ---
    FHktWorldState WorldState;
    FHktVMRuntimePool VMPool;

    TArray<FHktVMHandle> ActiveVMs;
    TArray<FHktVMHandle> CompletedVMs;
    TArray<FHktPhysicsEvent> GeneratedPhysicsEvents;

    // --- Systems ---
    FHktEntityArrangeSystem EntityArrangeSystem;
    FHktVMBuildSystem       VMBuildSystem;
    FHktVMProcessSystem     VMProcessSystem;
    FHktPhysicsSystem       PhysicsSystem;
    FHktApplyStoreSystem    ApplyStoreSystem;
    FHktVMCleanupSystem     VMCleanupSystem;
    FHktPublishRenderSystem PublishRenderSystem;

    TUniquePtr<class FHktVMInterpreter> Interpreter;
};