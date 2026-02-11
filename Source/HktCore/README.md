HktCore 시뮬레이션 모듈 아키텍처 설계서

작성일: 2026년 2월 11일
대상: HktCore 모듈 구현 담당자
플랫폼: Unreal Engine 5.6 (C++ Module)

1. 개요 (Overview)

본 문서는 HktCore 모듈의 핵심 시뮬레이션 엔진에 대한 설계 명세를 기술합니다. 이 시스템은 결정론적(Deterministic) 결과를 보장하며, 롤백(Rollback) 기반의 네트워크 동기화를 지원하기 위해 **데이터(State)**와 **로직(System)**을 엄격하게 분리하는 구조를 따릅니다.

1.1 핵심 목표

순수 C++ 시뮬레이션: 언리얼 엔진의 UObject 라이프사이클에 의존하지 않는 독립적인 메모리 관리.

결정론적 실행: 동일한 초기 상태(WorldState)와 입력(Input)이 주어지면, 언제나 동일한 결과(Next State)를 보장.

VM 기반 로직: 게임플레이 로직은 바이트코드 기반의 VM에서 실행되며, 로직 실행 중 월드 상태를 직접 오염시키지 않음.

2. 아키텍처 원칙 (Core Principles)

2.1 상태와 로직의 분리 (Separation of State and Logic)

State (FHktWorldState): 시뮬레이션의 모든 데이터(Entity, Position, Properties)를 포함합니다. 로직을 포함하지 않으며, 손쉽게 복사(Deep Copy) 및 직렬화가 가능해야 합니다.

System: 데이터를 입력받아 가공하는 로직 처리기입니다. 내부에 상태를 저장하지 않습니다(Stateless).

2.2 트랜잭션 기반 상태 변경 (Buffered Writes)

VM이나 로직은 FHktWorldState를 직접 수정하지 않습니다.

변경 사항은 FHktVMStore (혹은 LocalContext)라는 로컬 버퍼에 기록되며, 프레임의 특정 시점(ApplyStoreSystem)에 일괄 적용(Commit)됩니다.

3. 데이터 구조 명세 (Data Structures - Mandatory Spec)

구현자는 아래 데이터 구조의 필드 구성과 타입을 준수해야 합니다.

3.1 Event Structures

FHktEvent (범용 게임플레이 이벤트)

EventTag (FGameplayTag): 이벤트 종류.

SourceEntity (int32): 유발자 ID.

TargetEntity (int32): 대상 ID.

Location (FVector): 발생 위치.

Param0, Param1 (int32): 범용 정수 파라미터.

FHktPhysicsEvent (물리 충돌 이벤트)

EntityA, EntityB (int32): 충돌한 두 엔티티.

ContactPoint (FVector): 충돌 지점.

3.2 State Structures

FHktEntityState

Entity (int32): 엔티티 ID (Invalid = -1).

Properties (TArray<int8>): 중요 속성값은 바이트 배열로 관리하여 메모리 연속성을 보장합니다.

Tags (FGameplayTagContainer): 상태 태그.

FHktSimulationState

FrameNumber (int64)

RandomSeed (int32)

Entities (TArray<FHktEntityState>): 빠른 순회를 위한 배열 구조 권장.

4. VM 런타임 규격 (VM Runtime Specification)

VM 구현 시 아래의 메모리 레이아웃과 상태 머신을 반드시 따라야 합니다.

4.1 VM 상태 (State Machine)

EVMStatus 열거형을 정의하고 다음 상태 전이를 구현하십시오.

Ready: 초기화 직후.

Running: 실행 중.

Yielded: WaitFrames가 남아있어 일시 중단됨.

WaitingEvent: 외부 이벤트를 대기 중.

Completed: 정상 종료.

Failed: 오류로 인한 종료.

4.2 런타임 컨텍스트 (FHktVMRuntime)

SOA(Structure of Arrays) 풀링을 위해 아래 데이터는 POD(Plain Old Data)에 가깝게 유지되어야 합니다.

Program Counter (PC): int32

Registers: int32 Registers[16] (R0 ~ R15 고정 크기 배열).

실수형 접근 헬퍼 GetRegFloat / SetRegFloat (reinterpret_cast 사용) 제공 필수.

Timers: CreationFrame (생성 시점), WaitFrames (Yield 잔여 프레임).

Search Result: FSpatialQueryResult (공간 검색 결과 캐싱).

4.3 핸들 규격 (FHktVMHandle)

VM 인스턴스에 대한 참조는 포인터가 아닌 아래 비트필드 구조체로 관리합니다.

Index : 24 bits (최대 16M 슬롯)

Generation : 8 bits (ABA 문제 방지)

유효성 검사: Index != 0xFFFFFF

5. 실행 파이프라인 (Execution Pipeline)

FHktSimulationWorld::ProcessBatch 함수 내에서 매 프레임 실행되는 순차적 단계입니다.

Phase 1: 준비 (Preparation)

Arrange System: 삭제된 소유자 정리.

Build System: FHktEvent를 순회하며 FHktVMRuntimePool::Allocate()를 통해 VM을 생성 및 레지스터(R0~R3 등) 초기화.

Phase 2: 실행 (Execution)

Process System:

활성 VM 루프:

WaitingEvent: 타이머 감소 또는 외부 이벤트 매칭 확인.

Yielded: WaitFrames 감소. 0이 되면 Ready로 전환.

Running/Ready: 인터프리터(Execute) 실행.

실행 결과(Status)에 따라 CompletedVMs 목록으로 이동.

Phase 3: 물리 및 적용 (Physics & Commit)

Physics System:

공간 분할(Spatial Hashing): 격자 크기 CellSize = 1000.0f 고정.

WorldToCell(Pos) 함수: FloorToInt(Pos / CellSize) 로직 사용.

위치가 변경된 엔티티만 GridMap 갱신 (Dirty Check).

충돌 발생 시 FHktPhysicsEvent 생성 및 위치 보정 로직 수행.

Apply Store System:

VM 실행 결과로 쌓인 PendingWrites를 WorldState에 최종 반영.

Phase 4: 정리 (Cleanup)

Cleanup System: 완료된 VM 핸들 해제 (Free).

6. 구현 시 주의사항 (Implementation Notes)

메모리 풀링 (Pooling): FHktVMRuntimePool은 MaxVMs(예: 256~1024) 크기의 정적 배열 혹은 동적 확장 가능한 배열과 FreeSlots 스택을 사용하여 O(1) 할당/해제를 구현해야 합니다.

결정론적 부동소수점: 물리 연산 시 FVector 연산 순서를 임의로 변경하지 마십시오.

Store 패턴: VM은 절대로 WorldState를 직접 수정하면 안 됩니다. 반드시 Context.Write()를 통해 버퍼링해야 합니다.

7. 폴더 구조 (Directory Structure)

HKTCore/
├── Public/
│   ├── HktCoreTypes.h         // FHktEvent, FHktEntityState 등 구조체 정의
│   ├── HktVMTypes.h           // FHktVMRuntime, FHktVMHandle, EVMStatus
│   ├── HktSimulationSystems.h // 각 단계별 시스템 클래스 정의
│   └── HktSimulationWorld.h   // 메인 월드 클래스
├── Private/
│   ├── HktSimulationSystems.cpp
│   ├── HktSimulationWorld.cpp
│   └── VM/                    // VM 인터프리터 상세 구현


승인: _________________ (Tech Lead)

*** 부록: HktCore 핵심 클래스 명세 (C++ Header Specifications) ***

구현 시 아래의 클래스 명세와 데이터 레이아웃을 준수하여 작성해 주십시오.

1. HKTCore/Public/HktCoreTypes.h

기본 데이터 타입 및 상태 구조체 정의입니다.

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
// Entity & World State
// ============================================================================

struct HKTCORE_API FHktEntityState
{
    FHktEntityId EntityId = InvalidEntityId;
    FVector Position = FVector::ZeroVector;
    FGameplayTagContainer Tags;
    
    // 메모리 연속성을 위해 바이트 배열로 속성 관리
    TArray<int8> Properties; 
};

/** 시뮬레이션의 전체 스냅샷 (Deep Copy 및 Rollback 지원 필수) */
struct HKTCORE_API FHktWorldState
{
    int64 FrameNumber = 0;
    int32 RandomSeed = 0;
    
    // Entity Storage
    TMap<FHktEntityId, FHktEntityState> Entities;
    
    // Helpers
    FHktEntityState* GetEntityMutable(FHktEntityId Id) { return Entities.Find(Id); }
    const FHktEntityState* GetEntity(FHktEntityId Id) const { return Entities.Find(Id); }
    void RemoveEntity(FHktEntityId Id) { Entities.Remove(Id); }
    
    void CopyFrom(const FHktWorldState& Other)
    {
        FrameNumber = Other.FrameNumber;
        RandomSeed = Other.RandomSeed;
        Entities = Other.Entities;
    }
};

/** 렌더링용 보간 상태 */
struct HKTCORE_API FHktRenderState
{
    int64 FrameNumber = 0;
    TArray<FHktEntityState> InterpolatedEntities;
};


2. HKTCore/Public/HktVMTypes.h

VM 런타임, 핸들, 상태 정의입니다.

// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"

// ============================================================================
// VM Definitions
// ============================================================================

enum class EVMStatus : uint8
{
    Ready,
    Running,
    Yielded,
    WaitingEvent,
    Completed,
    Failed
};

struct FHktVMProgram
{
    FGameplayTag Tag;
    TArray<uint8> ByteCode;
    // ... Metadata
};

/** VM 로컬 변경 사항 버퍼 (WorldState 직접 수정 금지) */
struct FHktVMStore
{
    struct FPendingWrite
    {
        FHktEntityId Entity;
        int32 PropertyId;
        int32 Value;
    };
    TArray<FPendingWrite> PendingWrites;

    void Write(FHktEntityId Entity, int32 PropId, int32 Val)
    {
        PendingWrites.Add({Entity, PropId, Val});
    }
    
    void Clear() { PendingWrites.Reset(); }
};

// ============================================================================
// Runtime Context
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
    
    // Context Info
    FHktEntityId SelfEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    
    // Spatial Query Result Cache
    // FSpatialQueryResult SpatialQuery; // 필요 시 주석 해제하여 사용

    bool IsRunnable() const { return Status == EVMStatus::Ready || Status == EVMStatus::Running; }
    
    // Register Helpers
    void SetRegFloat(int32 Idx, float Val) 
    { 
        check(Idx < MaxRegisters); 
        Registers[Idx] = *reinterpret_cast<int32*>(&Val); 
    }
    float GetRegFloat(int32 Idx) const 
    { 
        check(Idx < MaxRegisters); 
        return *reinterpret_cast<const float*>(&Registers[Idx]); 
    }
};

/** 24bit Index + 8bit Generation Handle */
struct FHktVMHandle
{
    uint32 Index : 24;
    uint32 Generation : 8;
    
    bool IsValid() const { return Index != 0xFFFFFF; }
    static FHktVMHandle Invalid() { return {0xFFFFFF, 0}; }
    
    bool operator==(const FHktVMHandle& Other) const 
    { 
        return Index == Other.Index && Generation == Other.Generation; 
    }
};

// ============================================================================
// VM Pool
// ============================================================================

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

각 단계별 로직 처리기(System) 인터페이스입니다.

// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktVMTypes.h"

// Forward Declarations
class FHktVMInterpreter;

/** 1. Entity Arrange System: 제거된 소유자 정리 */
struct HKTCORE_API FHktEntityArrangeSystem
{
    void Process(FHktWorldState& WorldState, const TArray<int64>& RemovedOwnerIds);
};

/** 2. VM Build System: 이벤트 -> VM 생성 */
struct HKTCORE_API FHktVMBuildSystem
{
    void Process(
        const TArray<FHktEvent>& Events, 
        int32 CurrentFrame, 
        FHktVMRuntimePool& Pool, 
        TArray<FHktVMHandle>& OutActiveVMs
    );
};

/** 3. VM Process System: 바이트코드 실행 */
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

/** 4. Physics System: 공간 분할 및 충돌 감지 */
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
    
    void Process(
        FHktWorldState& WorldState, 
        TArray<FHktPhysicsEvent>& OutPhysicsEvents
    );
};

/** 5. Apply Store System: 변경 사항(Store) 커밋 */
struct HKTCORE_API FHktApplyStoreSystem
{
    void Process(
        FHktWorldState& WorldState, 
        const TArray<FHktVMHandle>& DirtyVMs,
        FHktVMRuntimePool& Pool
    );
};

/** 6. VM Cleanup System: 종료된 VM 해제 */
struct HKTCORE_API FHktVMCleanupSystem
{
    void Process(TArray<FHktVMHandle>& CompletedVMs, FHktVMRuntimePool& Pool);
};

/** 7. Publish System: 렌더링 상태 발행 */
struct HKTCORE_API FHktPublishRenderSystem
{
    void Process(const FHktWorldState& WorldState, FHktRenderState& OutRenderState);
};


4. HKTCore/Public/HktSimulationWorld.h

전체 파이프라인을 조율하는 메인 클래스입니다.

// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"
#include "HktVMTypes.h"
#include "HktSimulationSystems.h"

/**
 * FHktSimulationWorld
 * - 시뮬레이션의 진입점(Entry Point)이자 파사드(Facade)
 * - ProcessBatch() 내에서 결정론적 순서 보장 필수
 */
class HKTCORE_API FHktSimulationWorld
{
public:
    FHktSimulationWorld();
    ~FHktSimulationWorld();

    /** 메인 틱 함수: Input -> Build -> Process -> Physics -> Commit -> Cleanup */
    void ProcessBatch(const FHktSimulationEvent& Event);

    /** 롤백 지원: 특정 상태로 복구 */
    void RestoreState(const FHktWorldState& InState);

    /** 스냅샷 추출 */
    void GetStateSnapshot(FHktWorldState& OutState) const;

    /** 렌더링 상태 발행 */
    void PublishRenderState(FHktRenderState& OutState);

private:
    // --- Data ---
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

    // --- Interpreter ---
    TUniquePtr<class FHktVMInterpreter> Interpreter;
};
