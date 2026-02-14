HktCore 시뮬레이션 모듈 아키텍처 설계서

작성일: 2026년 2월 11일
최종 수정: 2026년 2월 14일 (SOA 리팩토링 반영)
대상: HktCore 모듈 구현 담당자
플랫폼: Unreal Engine 5.6 (C++ Module)

1. 개요 (Overview)

본 문서는 HktCore 모듈의 핵심 시뮬레이션 엔진에 대한 설계 명세를 기술합니다. 이 시스템은 결정론적(Deterministic) 결과를 보장하며, 롤백(Rollback) 기반의 네트워크 동기화를 지원하기 위해 **데이터(State)**와 **로직(System)**을 엄격하게 분리하는 구조를 따릅니다.

1.1 핵심 목표

순수 C++ 시뮬레이션: 언리얼 엔진의 UObject 라이프사이클에 의존하지 않는 독립적인 메모리 관리.

결정론적 실행: 동일한 초기 상태(WorldState)와 입력(Input)이 주어지면, 언제나 동일한 결과(Next State)를 보장.

VM 기반 로직: 게임플레이 로직은 바이트코드 기반의 VM에서 실행되며, 로직 실행 중 월드 상태를 직접 오염시키지 않음.

SOA 데이터 레이아웃: 엔티티 데이터를 PropertyId별 컬럼으로 관리하여 캐시 효율을 극대화.

2. 아키텍처 원칙 (Core Principles)

2.1 상태와 로직의 분리 (Separation of State and Logic)

State (FHktWorldState): 시뮬레이션의 모든 데이터를 SOA 컬럼 기반으로 저장. 로직을 포함하지 않으며, Deep Copy 및 직렬화가 가능.

System: 데이터를 입력받아 가공하는 로직 처리기. 내부에 상태를 저장하지 않음(Stateless).

2.2 트랜잭션 기반 상태 변경 (Buffered Writes)

VM이나 로직은 FHktWorldState를 직접 수정하지 않습니다.

변경 사항은 FHktVMStore의 PendingWritesByProperty에 PropertyId별로 배치 기록되며, 프레임의 특정 시점(ApplyStoreSystem)에 컬럼 단위로 일괄 적용(Commit)됩니다.

2.3 SOA (Structure of Arrays) 레이아웃

기존 AOS(TMap<EntityId, EntityState>) 방식 대비:
- 같은 Property를 가진 데이터가 메모리에 연속 배치 → 캐시 라인 히트율 극대화
- 시스템이 필요한 컬럼만 접근 (예: Physics는 PosX/PosY/PosZ 컬럼만)
- 컬럼 포인터를 루프 밖에서 캐싱하여 TMap 룩업을 1회로 제한

3. 데이터 구조 명세 (Data Structures)

3.1 Event Structures

FHktEvent (범용 게임플레이 이벤트)

    EventId (int32): 이벤트 고유 ID.
    EventTag (FGameplayTag): 이벤트 종류.
    SourceEntity (int32): 유발자 ID.
    TargetEntity (int32): 대상 ID.
    Location (FVector): 발생 위치.
    Param0, Param1 (int32): 범용 정수 파라미터.

FHktPhysicsEvent (물리 충돌 이벤트)

    EntityA, EntityB (int32): 충돌한 두 엔티티.
    ContactPoint (FVector): 충돌 지점.

FHktSimulationEvent (프레임 단위 시뮬레이션 입력)

    FrameNumber (int64)
    RandomSeed (int32)
    DeltaSeconds (float)
    RemovedOwnerIds (TArray<int64>)
    Events (TArray<FHktEvent>)

3.2 SOA Data Column

FHktDataColumn — PropertyId별 int32 데이터 배열

    PropertyId (int32): 컬럼이 대응하는 Property ID.
    IntData (TArray<int32>): SlotIndex로 인덱싱되는 값 배열.
    GetInt(SlotIndex) / SetInt(SlotIndex, Value) 로 접근.

3.3 World State (SOA)

FHktWorldState — 시뮬레이션 전체 스냅샷

    FrameNumber (int64), RandomSeed (int32), NextEntityId (int32)

    Entity Index Mapping:
        EntityToIndex (TArray<int32>): EntityId -> SlotIndex (-1 = invalid)
        IndexToEntity (TArray<FHktEntityId>): SlotIndex -> EntityId
        FreeIndices (TArray<int32>): 재사용 가능 슬롯 스택

    SOA Data:
        Columns (TMap<int32, FHktDataColumn>): PropertyId별 데이터 컬럼
        TagColumn (TArray<TArray<int32>>): SlotIndex별 TagIndices

    Active Events:
        ActiveEvents (TArray<FHktEvent>): 진행 중인 이벤트 (중간 합류 클라이언트 동기화용)

    Core Operations:
        AllocateEntity() -> FHktEntityId
        RemoveEntity(Id)
        IsValidEntity(Id) -> bool
        GetIndex(Id) -> int32 (EntityId → SlotIndex, invalid이면 -1)
        GetEntityCount() -> int32

    Property Access:
        GetProperty(Entity, PropertyId) -> int32
        SetProperty(Entity, PropertyId, Value)
        GetColumn(PropertyId) -> const FHktDataColumn*
        GetOrCreateColumn(PropertyId) -> FHktDataColumn&

    Iteration:
        ForEachEntity([](FHktEntityId Id, int32 SlotIndex) { ... })

    DTO 변환 (HktRuntime 네트워크 직렬화용):
        ExtractEntityState(Id) -> FHktEntityState

    Snapshot/Rollback:
        CopyFrom(Other), operator<< 직렬화

3.4 Entity State (DTO)

FHktEntityState — HktRuntime 모듈의 네트워크/DB 직렬화 전용 DTO

    EntityId (int32)
    Position (FVector): PosX/PosY/PosZ에서 조립
    TagIndices (TArray<int32>): 커스텀 태그 시스템 인덱스
    Properties (TArray<int32>): PropertyId 인덱스 기반 속성 배열

    주의: HktCore 내부에서는 SOA WorldState를 직접 사용.
    FHktEntityState는 ExtractEntityState()를 통해 SOA -> DTO 변환 시에만 생성.

3.5 World View (Zero Copy)

FHktWorldView — WorldState의 경량 읽기 뷰 (Zero Copy + Sparse Overlay)

    WorldState (const FHktWorldState*): 원본 데이터 참조 (복사 없음)
    IntOverlays (TMap<PropId, TMap<EntityId, int32>>): Property-first 희소 오버레이

    GetColumn(PropertyId): WorldState 컬럼 직접 접근 (Zero Copy — 벌크 순회용)
    GetOverlay(PropertyId): 해당 Property의 Overlay 맵 (없으면 nullptr)
    GetInt(Entity, PropertyId): Overlay → WorldState 순서로 layered read (단발성 조회용)
    GetAllEntities(): WorldState->IndexToEntity 참조 반환

    PublishViewSystem이 ActiveVMs의 Store를 순회하여 Overlay를 자동 구축

3.6 Property IDs (HktPropertyIds.h)

PropertyId 네임스페이스에 uint16 상수로 정의:

    위치/이동: PosX(0), PosY(1), PosZ(2), RotYaw(3), MoveTargetX/Y/Z(4-6), MoveSpeed(7), IsMoving(8)
    전투/상태: Health(10), MaxHealth(11), AttackPower(12), Defense(13), Team(14), Mana(15), MaxMana(16)
    소유/타입: OwnerEntity(20), EntityType(21)
    이벤트 파라미터: TargetPosX/Y/Z(30-32), Param0-3(33-36)
    애니메이션: AnimState(40), VisualState(41)
    소유권: OwnerPlayerHash(52)

4. VM 런타임 규격 (VM Runtime Specification)

4.1 VM 상태 (State Machine)

EVMStatus:
    Ready -> Running -> Completed / Failed
    Running -> Yielded (WaitFrames > 0)
    Running -> WaitingEvent (외부 이벤트 대기)
    Yielded -> Ready (WaitFrames == 0)
    WaitingEvent -> Ready (이벤트 매칭 또는 타이머 만료)

4.2 런타임 컨텍스트 (FHktVMRuntime)

    Program Counter (PC): int32
    Registers: int32[16] (R0~R9 범용, R10~R15 특수: Self, Target, Spawned, Hit, Iter, Flag)
    Status: EVMStatus
    Timers: CreationFrame, WaitFrames
    Search Result: FSpatialQueryResult (공간 검색 결과 캐싱)
    Store: FHktVMStore* (로컬 버퍼 참조)

4.3 VM Store (SOA 배치 쓰기)

FHktVMStore:
    SourceEntity, TargetEntity: 현재 VM 컨텍스트
    PendingWritesByProperty (TMap<uint16, TArray<FPendingWrite>>):
        PropertyId별로 쓰기를 묶어 배치 처리
        FPendingWrite { Entity, Value }
    LocalCache (TMap<uint64, int32>): read-after-write 일관성
    WorldState (const FHktWorldState*): SOA 읽기 참조

    Read 순서: LocalCache -> WorldState.GetProperty()
    Write: LocalCache + PendingWritesByProperty에 기록

4.4 핸들 규격 (FHktVMHandle)

    Index : 24 bits (최대 16M 슬롯)
    Generation : 8 bits (ABA 문제 방지)
    유효성 검사: Index != 0xFFFFFF

5. 실행 파이프라인 (Execution Pipeline)

FHktSimulationWorld::ProcessBatch 함수 내에서 매 프레임 실행되는 순차적 단계입니다.

Phase 1: 준비 (Preparation)

    1-1. Arrange System: 삭제된 소유자에 속하는 엔티티 정리.
        - OwnerPlayerHash 컬럼을 루프 밖에서 캐싱하여 순회.

    1-2. Build System: FHktEvent를 순회하며 VM 생성 및 레지스터 초기화.
        - VM 생성 성공 시 WorldState.ActiveEvents에 이벤트 등록.

Phase 2: 실행 (Execution)

    Process System:
        활성 VM 루프:
            WaitingEvent: 타이머 감소 또는 외부 이벤트 매칭.
            Yielded: WaitFrames 감소. 0이 되면 Ready 전환.
            Running/Ready: 인터프리터(Execute) 실행.
        실행 결과에 따라 CompletedVMs 목록으로 이동.

Phase 3: 물리 및 적용 (Physics & Commit)

    3-1. Physics System:
        - 공간 분할(Spatial Hashing): CellSize = 1000.0f
        - RebuildGrid: PosX/PosY 컬럼 포인터를 캐싱하여 순회 (Z는 2D 그리드에 불필요)
        - 충돌 감지: PosX/PosY/PosZ 컬럼 포인터를 캐싱, EntityToIndex로 직접 인덱싱
        - PhysicsEvent -> PendingExternalEvents 변환 (양방향 등록)

    3-2. Apply Store System:
        - VM별 PendingWritesByProperty를 순회
        - PropertyId별로 GetOrCreateColumn 후 해당 컬럼에 순차 기록
        - 캐시 효율: 같은 PropertyId의 모든 writes가 하나의 컬럼 메모리에 연속 접근

Phase 4: 정리 (Cleanup)

    Cleanup System: 완료된 VM 핸들 해제.
        - WorldState.ActiveEvents에서 해당 이벤트 제거 (SourceEntity + EventTag 매칭).

Phase 5: 뷰 생성 (View)

    CreateWorldView(OutView):
        - PublishViewSystem을 통해 FHktWorldView를 초기화.
        - WorldState 포인터를 연결 (Zero Copy — 데이터 복사 없음).
        - ActiveVMs의 Store를 순회하며 Int/Float Overlay를 구축.
        - 렌더러는 GetInt/GetFloat으로 Overlay → WorldState 순서로 조회.

6. 캐시 효율 패턴 (Cache Efficiency Patterns)

6.1 컬럼 포인터 호이스팅

모든 시스템에서 ForEachEntity 루프 전에 GetColumn()으로 컬럼 포인터를 캐싱합니다.
루프 내부에서는 Col->GetInt(SlotIndex) 배열 인덱싱만 수행하여 TMap 룩업을 제거합니다.

    // Good: TMap 룩업 1회
    const FHktDataColumn* ColX = WorldState.GetColumn(PropertyId::PosX);
    WorldState.ForEachEntity([&](FHktEntityId Id, int32 SlotIndex) {
        float X = ColX ? static_cast<float>(ColX->GetInt(SlotIndex)) : 0.f;
    });

    // Bad: 엔티티당 TMap 룩업
    WorldState.ForEachEntity([&](FHktEntityId Id, int32 SlotIndex) {
        float X = static_cast<float>(WorldState.GetProperty(Id, PropertyId::PosX));
    });

6.2 GetProperty vs GetColumn 사용 기준

    GetProperty(Entity, PropId): VM Store 내부, 단발성 읽기, 외부 API 등 편의성이 필요한 곳
    GetColumn(PropId) + Col->GetInt(Slot): 시스템 루프 내부, 성능이 중요한 벌크 순회

7. 구현 시 주의사항 (Implementation Notes)

메모리 풀링 (Pooling): FHktVMRuntimePool은 MaxVMs(256) 크기의 배열과 FreeSlots 스택을 사용하여 O(1) 할당/해제.

결정론적 부동소수점: 물리 연산 시 FVector 연산 순서를 임의로 변경하지 않음.

Store 패턴: VM은 절대로 WorldState를 직접 수정하면 안 됨. 반드시 Store.Write() / Store.WriteEntity()를 통해 버퍼링.

ActiveEvents 관리: VMBuildSystem에서 Add, VMCleanupSystem에서 Remove. WorldState에 포함되어 직렬화/롤백 시 자동 동기화.

FHktEntityState는 DTO: HktCore 내부에서는 SOA WorldState를 직접 사용. FHktEntityState는 HktRuntime의 네트워크 직렬화 시 ExtractEntityState()로 생성.

8. 폴더 구조 (Directory Structure)

HktCore/
├── Public/
│   ├── HktCoreTypes.h         // FHktEvent, FHktEntityState, FHktDataColumn, FHktWorldState, FHktWorldView
│   ├── HktPropertyIds.h       // PropertyId 네임스페이스 (uint16 상수)
│   └── HktSimulator.h         // IHktSimulator 인터페이스
├── Private/
│   ├── HktCoreTypes.cpp       // FHktWorldState 구현부 (AllocateEntity, ExtractEntityState, 직렬화 등)
│   ├── HktSimulationSystems.h // 각 단계별 시스템 클래스 선언
│   ├── HktSimulationSystems.cpp
│   ├── HktSimulationWorld.h   // FHktSimulationWorld 클래스 선언
│   ├── HktSimulationWorld.cpp // 파이프라인 조율
│   ├── HktVMTypes.h           // EVMStatus, FHktVMRuntime, FHktVMHandle, FHktVMRuntimePool
│   └── VM/
│       ├── HktVMStore.h       // FHktVMStore (SOA 배치 쓰기)
│       ├── HktVMStore.cpp
│       ├── HktVMInterpreter.h
│       ├── HktVMInterpreter.cpp
│       ├── HktVMInterpreterActions.cpp
│       └── HktVMProgram.h     // FHktVMProgram, FHktVMProgramRegistry
