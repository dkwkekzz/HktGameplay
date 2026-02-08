HktCore 프로젝트 및 객체 구조 설계 가이드 (Rev. 4)

1. 프로젝트 목적

Core (Minimal)

순수 C++ 시뮬레이션 로직, VM, Physics(Spatial), 결정론적 상태 관리. (UObject 금지)

2. 물리적 폴더 구조 (Directory Structure)

Plugins/HktPlugin/Source/ 하위의 구조입니다.

HktPlugin/
├── HktCore/                    # [Pure C++] 시뮬레이션 코어
│   ├── Public/
│   │   ├── VM/                 # 가상 머신 관련
│   │   │   ├── HktVMProcessor.h    # VM 코어 (Intent/System 이벤트 처리기)
│   │   │   ├── HktInstruction.h    # 명령어 셋 (ISA)
│   │   │   └── HktProgramRegistry.h
│   │   ├── State/              # 데이터 저장소
│   │   │   ├── HktWorldState.h     # [Global] 확정된 전역 상태
│   │   │   ├── HktLocalContext.h   # [Local] 트랜잭션 (Scratchpad)
│   │   │   └── HktComponentTypes.h 
│   │   ├── Physics/            # 물리 및 공간 관리
│   │   │   ├── HktSpatialSystem.h  # Cell 기반 공간 관리
│   │   │   └── HktCollisionShapes.h
│   │   ├── World/              # 시뮬레이션 월드
│   │   │   ├── HktSimulationWorld.h
│   │   │   └── HktEntityManager.h
│   │   └── Common/
│   │       ├── HktTypes.h          # 기본 타입 (Vector3, EntityID)
│   │       ├── HktEvents.h         # [NEW] IntentEvent 및 SystemEvent 정의
│   │       └── HktDeterministicMath.h
│   └── Private/
│       └── ...


3. 핵심 객체 계층 구조 (Class Hierarchy)

3.1. HktCore (Simulation Layer)

A. Event System (이벤트 시스템)

이벤트를 외부용과 내부용으로 분리하여 네트워크 효율성과 로직 유연성을 확보합니다.

FHktIntentEvent (External/Networked)

출처: 언리얼 PlayerController (유저 입력).

특징: 네트워크 리플리케이션 대상이며, 시뮬레이션의 **입력(Input)**이 됩니다.

용도: 이동 명령, 스킬 시전, 아이템 사용 등.

FHktSystemEvent (Internal/Local)

출처: 시뮬레이션 내부 로직 (물리 충돌, 타이머 만료, AI 트리거).

특징: 네트워크로 전송되지 않으며, 해당 프레임 내에서 즉시 소비되거나 다음 프레임 로직으로 이월됩니다.

용도: 충돌 반응(넉백/피격), 쿨타임 종료 알림, 연쇄 폭발 등.

B. State Management (데이터 관리)

FHktWorldState (Global Repository)

모든 엔티티의 확정된(Committed) 데이터 저장소입니다.

외부(HktUnreal)는 오직 이 객체를 통해 렌더링 데이터를 읽습니다.

FHktLocalContext (Transactional Storage)

VM 실행 중의 임시 작업 공간입니다.

Write는 이곳에 기록되며, 실행이 성공적으로 끝나면 Commit을 통해 WorldState에 반영됩니다.

C. VM Execution (로직 실행)

FHktVMProcessor (Persistent CPU Core)

Zero-Allocation: 매 이벤트마다 FHktExecutionState(레지스터/스택)를 리셋하여 재사용합니다.

Dual Pipeline:

FlushEventQueue(): IntentEvent(입력)를 일괄 처리합니다.

ExecuteImmediate(): SystemEvent(충돌 등)를 즉시 실행합니다.

D. Physics & Spatial (물리)

FHktSpatialSystem

Cell 기반 공간 분할을 사용하여 충돌 감지(Detection) 및 네트워크 관심 영역(Relevance)을 관리합니다.

충돌 발생 시 직접 데이터를 수정하지 않고, SystemEvent를 생성하여 VM에 위임합니다.

4. 데이터 흐름과 수명 주기 (Runtime Flow)

4.1. 시뮬레이션 루프 (Phased Execution)

HktSimulationWorld::Tick은 결정론적 순서를 보장하기 위해 3단계로 실행됩니다.

[Phase 1] Input Processing (Intent)

VMProcessor가 외부에서 수신된 FHktIntentEvent 큐를 모두 비웁니다.

이동, 스킬 사용 등의 1차적인 상태 변화가 WorldState에 반영됩니다.

[Phase 2] Spatial Update & Collision

변경된 위치 정보를 SpatialSystem에 업데이트합니다.

충돌이 감지되면, 이를 해결하기 위한 FHktSystemEvent 리스트를 생성합니다. (예: Tag: ON_COLLISION)

[Phase 3] Reaction Processing (System)

생성된 FHktSystemEvent들을 VMProcessor.ExecuteImmediate()를 통해 즉시 실행합니다.

이를 통해 1프레임 지연 없이 충돌 반응(체력 감소, 넉백 등)이 확정됩니다.

4.2. Unreal Bridge (Visualization)

HktUnreal 모듈은 모든 Phase가 끝난 후 WorldState를 읽어 렌더링을 업데이트합니다.

5. 설계 시 주의사항

Event Distinction (이벤트 구분)

IntentEvent는 무겁습니다(직렬화). 내부 로직에서는 반드시 가벼운 SystemEvent를 사용하십시오.

Deterministic Time

Tick은 DeltaTime 대신 FrameNumber를 인자로 받습니다. 내부적으로 고정 타임스텝(FixedTimeStep)을 곱하여 시간을 계산합니다.

*** 코드 구조 ***
아래 코드 형태로 구조를 재정립하고 이후 여기에 구현을 채워나가는 식으로 구현해보자.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/Map.h"

namespace Hkt
{
    // ==================================================================================
    // [Part 1] Common Types & Events (기본 자료형 및 이벤트 정의)
    // 경로: HktCore/Public/Common/HktTypes.h, HktEvents.h
    // ==================================================================================

    using FEntityID = uint32;
    constexpr FEntityID InvalidEntityID = 0;

    struct FVector3
    {
        float X, Y, Z; 
        FVector3() : X(0), Y(0), Z(0) {}
        FVector3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
        bool operator==(const FVector3& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }
    };

    struct FCellCoord
    {
        int32 X, Y;
        bool operator==(const FCellCoord& Other) const { return X == Other.X && Y == Other.Y; }
        friend uint32 GetTypeHash(const FCellCoord& C) { return HashCombine(::GetTypeHash(C.X), ::GetTypeHash(C.Y)); }
    };

    /**
     * @brief [External] 외부 입력 이벤트
     * - PlayerController -> HktUnreal -> HktCore로 전달되는 유저 의도(Intent)
     * - 네트워크 리플리케이션 대상 (직렬화 비용 발생)
     */
    struct FHktIntentEvent
    {
        uint32 EventTag;       // OpCode
        FEntityID Source;
        FEntityID Target;
        FVector3 Location;
        TArray<uint8> Payload; // Params (Serialized)
    };

    /**
     * @brief [Internal] 내부 시스템 이벤트
     * - 물리 충돌, 타이머 등 시뮬레이션 내부에서 발생하는 로직 트리거
     * - 네트워크 전송 X, 로컬에서 즉시 소비됨 (직렬화 비용 최소화 가능)
     */
    struct FHktSystemEvent
    {
        uint32 EventTag;       // OpCode
        FEntityID Source;
        FEntityID Target;
        FVector3 Location;
        TArray<uint8> Payload; // Params (Runtime Memory)
    };

    // SOA 구조의 컴포넌트 데이터 예시
    struct FComponentData
    {
        FVector3 Position;
        int32 Health;
    };

    // 물리 충돌 쌍
    struct FCollisionPair
    {
        FEntityID EntityA;
        FEntityID EntityB;
    };


    // ==================================================================================
    // [Part 2] State Management (데이터 저장소)
    // 경로: HktCore/Public/State/HktWorldState.h, HktLocalContext.h
    // ==================================================================================

    class FHktWorldState
    {
    public:
        const FComponentData* GetComponent(FEntityID EntityID) const
        {
            return EntityComponents.Find(EntityID);
        }

        void CommitComponent(FEntityID EntityID, const FComponentData& NewData)
        {
            EntityComponents.Add(EntityID, NewData);
        }

        const TMap<FEntityID, FComponentData>& GetAllEntities() const { return EntityComponents; }

    private:
        TMap<FEntityID, FComponentData> EntityComponents;
    };

    class FHktLocalContext
    {
    public:
        FHktLocalContext(FHktWorldState* InWorldState) : WorldState(InWorldState) {}

        const FComponentData& Read(FEntityID EntityID)
        {
            if (FComponentData* Cached = LocalCache.Find(EntityID)) return *Cached;
            if (const FComponentData* Original = WorldState->GetComponent(EntityID)) return LocalCache.Add(EntityID, *Original);
            static FComponentData DefaultData; 
            return DefaultData;
        }

        void Write(FEntityID EntityID, const FComponentData& InData)
        {
            LocalCache.Add(EntityID, InData);
            DirtyEntities.AddUnique(EntityID);
        }

        void CommitChanges()
        {
            for (FEntityID ID : DirtyEntities)
            {
                WorldState->CommitComponent(ID, LocalCache[ID]);
            }
            LocalCache.Empty();
            DirtyEntities.Empty();
        }

    private:
        FHktWorldState* WorldState;
        TMap<FEntityID, FComponentData> LocalCache;
        TArray<FEntityID> DirtyEntities;
    };


    // ==================================================================================
    // [Part 3] Physics & Spatial (공간 관리)
    // 경로: HktCore/Public/Physics/HktSpatialSystem.h
    // ==================================================================================

    class FHktSpatialSystem
    {
    public:
        FHktSpatialSystem() : CellSize(1000.0f) {}

        void UpdateEntityPosition(FEntityID EntityID, const FVector3& OldPos, const FVector3& NewPos)
        {
            FCellCoord OldCell = WorldToCell(OldPos);
            FCellCoord NewCell = WorldToCell(NewPos);
            if (!(OldCell == NewCell))
            {
                GridMap.FindOrAdd(OldCell).Remove(EntityID);
                GridMap.FindOrAdd(NewCell).Add(EntityID);
            }
        }

        void UpdateAndDetectCollisions(const FHktWorldState& State, TArray<FCollisionPair>& OutCollisions)
        {
            for (auto& Pair : GridMap)
            {
                const TArray<FEntityID>& Entities = Pair.Value;
                for (int32 i = 0; i < Entities.Num(); ++i)
                {
                    for (int32 j = i + 1; j < Entities.Num(); ++j)
                    {
                        // Broadphase -> Narrowphase Check
                        OutCollisions.Add({Entities[i], Entities[j]});
                    }
                }
            }
        }

        FCellCoord WorldToCell(const FVector3& Pos) const
        {
            return { FMath::FloorToInt(Pos.X / CellSize), FMath::FloorToInt(Pos.Y / CellSize) };
        }

    private:
        TMap<FCellCoord, TArray<FEntityID>> GridMap;
        float CellSize;
    };


    // ==================================================================================
    // [Part 4] VM Processor (VM 코어)
    // 경로: HktCore/Public/VM/HktVMProcessor.h
    // ==================================================================================

    struct FHktExecutionState
    {
        uint32 ProgramCounter;
        uint8  Registers[256];
        uint8  StackMemory[1024];
        int32  StackPointer;
        
        void Reset()
        {
            ProgramCounter = 0;
            StackPointer = 0;
        }
    };

    class FHktVMProcessor
    {
    public:
        FHktVMProcessor(FHktWorldState* InWorldState, FHktSpatialSystem* InSpatialSystem)
            : WorldState(InWorldState), SpatialSystem(InSpatialSystem) 
        {
            CoreState.Reset();
        }

        void EnqueueEvent(const FHktIntentEvent& Event)
        {
            EventQueue.Add(Event);
        }

        // [Phase 1] 입력 처리 (IntentEvent)
        void FlushEventQueue()
        {
            for (const FHktIntentEvent& Event : EventQueue)
            {
                ExecuteInternal(Event);
            }
            EventQueue.Reset();
        }

        // [Phase 3] 즉시 실행 (SystemEvent)
        void ExecuteImmediate(const FHktSystemEvent& Event)
        {
            ExecuteInternal(Event);
        }

    private:
        // 공통 실행 로직 (Template for Intent & System Event)
        template <typename EventType>
        void ExecuteInternal(const EventType& Event)
        {
            CoreState.Reset(); // 1. Reset VM
            FHktLocalContext Context(WorldState); // 2. Create Transaction
            RunExecutionLoop(Event, Context); // 3. Execute Bytecode
            Context.CommitChanges(); // 4. Commit
        }

        template <typename EventType>
        void RunExecutionLoop(const EventType& Event, FHktLocalContext& Context)
        {
            // TODO: Registry->GetProgram(Event.EventTag) -> Fetch/Decode/Execute
            // 예: 충돌 이벤트면 OnCollision 바이트코드 실행
        }

    private:
        FHktWorldState* WorldState;
        FHktSpatialSystem* SpatialSystem;
        FHktExecutionState CoreState; // Persistent State (Zero-Alloc)
        TArray<FHktIntentEvent> EventQueue; // Only Intents here
    };


    // ==================================================================================
    // [Part 5] Simulation World (메인 루프)
    // 경로: HktCore/Public/World/HktSimulationWorld.h
    // ==================================================================================

    class FHktSimulationWorld
    {
    public:
        FHktSimulationWorld() 
            : VMProcessor(&WorldState, &SpatialSystem)
        {}

        // Phased Execution Loop
        void Tick(uint32 FrameNumber)
        {
            // [Phase 1] Input Processing (Intent)
            // 외부 입력 처리 -> 위치 이동 발생
            VMProcessor.FlushEventQueue();

            // [Phase 2] Spatial & Physics
            // 이동한 위치 기반 충돌 감지 -> SystemEvent 생성
            TArray<FCollisionPair> Collisions;
            SpatialSystem.UpdateAndDetectCollisions(WorldState, Collisions);

            // [Phase 3] Reaction Processing (System)
            // 충돌 반응 즉시 실행 (1프레임 지연 방지)
            for (const auto& Pair : Collisions)
            {
                FHktSystemEvent HitEvent;
                HitEvent.EventTag = 999; // ON_COLLISION
                HitEvent.Source = Pair.EntityA;
                HitEvent.Target = Pair.EntityB;
                
                // SystemEvent는 큐에 넣지 않고 즉시 실행
                VMProcessor.ExecuteImmediate(HitEvent);
            }
        }

        void AddInputEvent(const FHktIntentEvent& Event)
        {
            VMProcessor.EnqueueEvent(Event);
        }

        const FHktWorldState& GetState() const { return WorldState; }

    private:
        FHktSpatialSystem SpatialSystem;
        FHktWorldState WorldState;
        FHktVMProcessor VMProcessor;
    };
}