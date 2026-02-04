# HktCore VM 아키텍처

이 문서는 HktCore의 가상 머신(VM) 시스템이 어떻게 작동하는지 상세히 설명합니다.

---

## 목차

1. [개요](#1-개요)
2. [핵심 데이터 타입](#2-핵심-데이터-타입)
3. [명령어 인코딩](#3-명령어-인코딩)
4. [레지스터 아키텍처](#4-레지스터-아키텍처)
5. [OpCode 시스템](#5-opcode-시스템)
6. [Flow 빌더 API](#6-flow-빌더-api)
7. [3단계 파이프라인](#7-3단계-파이프라인)
8. [Stash 시스템](#8-stash-시스템)
9. [실행 흐름 예시](#9-실행-흐름-예시)
10. [확장 가이드](#10-확장-가이드)

---

## 1. 개요

HktCore VM은 **결정론적 게임 로직 실행**을 위한 내장형 가상 머신입니다.

### 설계 원칙

- **Pure C++**: UObject/UWorld 참조 없음 - 네트워킹 코드와 완전 분리
- **결정론적**: 동일 입력(IntentEvent) → 동일 출력(상태 변경)
- **이벤트 기반**: 비동기 이벤트(충돌, 애니메이션) 대기 지원
- **메모리 효율**: SOA 레이아웃, 컴팩트한 바이트코드

### 전체 흐름도

```
┌─────────────────────────────────────────────────────────────┐
│                     FHktIntentEvent                         │
│  (EventTag, SourceEntity, TargetEntity, Location, Payload) │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              FHktVMProgramRegistry                          │
│         EventTag → FHktVMProgram (바이트코드)               │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    FHktVMProcessor                          │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐                │
│  │  Build   │ → │ Execute  │ → │ Cleanup  │                │
│  │ VM 생성  │   │ 바이트코드│   │ 결과 적용 │                │
│  └──────────┘   │   실행   │   └──────────┘                │
│                 └──────────┘                                │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    IHktStashInterface                       │
│  (MasterStash: 서버 / VisibleStash: 클라이언트)            │
│  엔티티 속성 저장 (SOA 레이아웃)                            │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. 핵심 데이터 타입

### EVMStatus

VM의 현재 실행 상태.

```cpp
enum class EVMStatus : uint8
{
    Ready,          // 실행 대기 (다음 Execute에서 실행)
    Running,        // 실행 중
    Yielded,        // yield 상태 (지정된 프레임 후 재개)
    WaitingEvent,   // 이벤트 대기 중 (충돌, 애니메이션 등)
    Completed,      // 정상 완료 (Halt 명령 실행)
    Failed,         // 오류로 중단
};
```

**상태 전이:**
```
Ready → Running → Yielded → Ready → Running → Completed
                ↘ WaitingEvent → Ready ↗
                          ↘ Failed
```

---

## 3. 명령어 인코딩

### 32비트 명령어 포맷

모든 명령어는 **4바이트**로 인코딩됩니다.

```
┌────────┬────┬────┬────┬────────────┐
│ OpCode │Dst │Src1│Src2│   Imm12    │  3-operand 형식
│  8bit  │4bit│4bit│4bit│   12bit    │
└────────┴────┴────┴────┴────────────┘

┌────────┬────┬──────────────────────┐
│ OpCode │Dst │       Imm20          │  Load immediate 형식
│  8bit  │4bit│       20bit          │
└────────┴────┴──────────────────────┘
```

---

## 4. 레지스터 아키텍처

### 16개 레지스터 (32비트)

| 레지스터 | 인덱스 | 용도 |
|----------|--------|------|
| R0~R8 | 0~8 | 범용 |
| Temp | 9 | 임시 (R9 별칭) |
| **Self** | 10 | IntentEvent.SourceEntity |
| **Target** | 11 | IntentEvent.TargetEntity |
| **Spawned** | 12 | 최근 SpawnEntity 결과 |
| **Hit** | 13 | WaitCollision 충돌 대상 |
| **Iter** | 14 | ForEach 순회 현재 엔티티 |
| **Flag/Count** | 15 | 조건 플래그 / 검색 결과 개수 |

### 특수 레지스터 자동 설정

```cpp
// VM 생성 시 자동 초기화
Runtime.Registers[Reg::Self] = Event.SourceEntity;
Runtime.Registers[Reg::Target] = Event.TargetEntity;

// SpawnEntity 실행 후
Runtime.Registers[Reg::Spawned] = NewEntityId;

// WaitCollision 이벤트 발생 시
Runtime.Registers[Reg::Hit] = CollidedEntityId;

// FindInRadius 후 NextFound 호출 시
Runtime.Registers[Reg::Iter] = FoundEntityId;
Runtime.Registers[Reg::Flag] = HasMore ? 1 : 0;
```

---

## 7. 3단계 파이프라인

`FHktVMProcessor`는 매 프레임 **Build → Execute → Cleanup** 순서로 실행됩니다.

### Phase 1: Build

IntentEvent를 받아 VM을 생성합니다.

```cpp
void FHktVMProcessor::Build(int32 CurrentFrame)
{
    // 1. 대기 중인 이벤트 가져오기
    TArray<FHktIntentEvent> Events = PullIntentEvents();

    for (const FHktIntentEvent& Event : Events)
    {
        // 2. EventTag로 프로그램 검색
        const FHktVMProgram* Program =
            FHktVMProgramRegistry::Get().FindProgram(Event.EventTag);

        if (!Program) continue;

        // 3. VM 할당 및 초기화
        FHktVMHandle Handle = RuntimePool.Allocate();
        FHktVMRuntime* Runtime = RuntimePool.Get(Handle);

        Runtime->Program = Program;
        Runtime->Store = &StorePool[Handle.Index];
        Runtime->PC = 0;
        Runtime->Status = EVMStatus::Ready;

        // 4. 특수 레지스터 초기화
        Runtime->SetRegEntity(Reg::Self, Event.SourceEntity);
        Runtime->SetRegEntity(Reg::Target, Event.TargetEntity);

        // 5. 이벤트 파라미터 → Store
        Runtime->Store->Write(PropertyId::TargetPosX, Event.Location.X);
        // ... Payload → Param0~3

        ActiveVMs.Add(Handle);
    }

    // 6. Yield된 VM 재활성화
    RuntimePool.ForEachActive([](auto& Handle, auto& Runtime) {
        if (Runtime.Status == EVMStatus::Yielded && Runtime.WaitFrames <= 0)
            Runtime.Status = EVMStatus::Ready;
        else
            Runtime.WaitFrames--;
    });
}
```

### Phase 2: Execute

모든 활성 VM을 실행합니다.

```cpp
void FHktVMProcessor::Execute(float DeltaSeconds)
{
    // 1. 타이머 업데이트 (WaitSeconds)
    RuntimePool.ForEachActive([&](auto& Handle, auto& Runtime) {
        if (Runtime.Status == EVMStatus::WaitingEvent &&
            Runtime.EventWait.Type == EWaitEventType::Timer)
        {
            Interpreter->UpdateTimer(Runtime, DeltaSeconds);
        }
    });

    // 2. 모든 활성 VM 실행
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
```

**인터프리터 실행 루프:**

```cpp
EVMStatus FHktVMInterpreter::Execute(FHktVMRuntime& Runtime)
{
    int32 InstructionCount = 0;

    while (InstructionCount < MaxInstructionsPerTick)  // 10,000 제한
    {
        if (Runtime.PC >= Runtime.Program->CodeSize())
            return EVMStatus::Completed;

        const FInstruction& Inst = Runtime.Program->Code[Runtime.PC++];
        InstructionCount++;

        EVMStatus Status = ExecuteInstruction(Runtime, Inst);

        // Running이 아니면 즉시 반환
        if (Status != EVMStatus::Running)
            return Status;
    }

    return EVMStatus::Yielded;  // 명령어 제한 도달
}
```

### Phase 3: Cleanup

완료된 VM의 변경사항을 적용하고 정리합니다.

```cpp
void FHktVMProcessor::Cleanup(int32 CurrentFrame)
{
    for (FHktVMHandle Handle : CompletedVMs)
    {
        // 1. 버퍼링된 Store 변경사항 적용
        ApplyStoreChanges(Handle);

        // 2. VM 해제 (풀로 반환)
        FinalizeVM(Handle);
    }

    CompletedVMs.Reset();
}

void FHktVMProcessor::ApplyStoreChanges(FHktVMHandle Handle)
{
    FHktVMRuntime* Runtime = RuntimePool.Get(Handle);
    FHktVMStore& Store = *Runtime->Store;

    // PendingWrites를 Stash에 일괄 적용
    for (const auto& Write : Store.PendingWrites)
    {
        Stash->SetProperty(Write.Entity, Write.PropertyId, Write.Value);
    }

    Store.ClearPendingWrites();
}
```

### 전체 Tick 흐름

```cpp
void FHktVMProcessor::Tick(int32 CurrentFrame, float DeltaSeconds)
{
    Build(CurrentFrame);        // IntentEvent → VM 생성
    Execute(DeltaSeconds);      // 바이트코드 실행
    Cleanup(CurrentFrame);      // 결과 적용 + VM 정리
}
```

---

## 8. Stash 시스템

### MasterStash vs VisibleStash

| 구분 | MasterStash | VisibleStash |
|------|-------------|--------------|
| 위치 | 서버 | 클라이언트 |
| 권한 | 읽기/쓰기 | 읽기/스냅샷 적용 |
| 스냅샷 | 생성 | 적용 |
| 추적 | 변경된 엔티티 추적 | 없음 |

---

## 9. 실행 흐름 예시

### 시나리오: 파이어볼 스킬 시전

```
Frame 20: IntentEvent 수신
┌─────────────────────────────────────────────────────────┐
│ EventId = 1001                                          │
│ EventTag = "Ability.Skill.Fireball"                     │
│ SourceEntity = 10 (마법사)                              │
│ TargetEntity = 99 (더미)                                │
│ Location = (100, 0, 500)                                │
└─────────────────────────────────────────────────────────┘

=== PHASE 1: BUILD ===
├─ FindProgram("Ability.Skill.Fireball") → Fireball 바이트코드
├─ Allocate Handle(Index=5, Gen=1)
├─ Initialize Runtime[5]:
│     Program = Fireball 바이트코드
│     PC = 0
│     Registers[Self] = 10
│     Registers[Target] = 99
│     Store.TargetPosX/Y/Z = (100, 0, 500)
└─ ActiveVMs += Handle(5)

=== PHASE 2: EXECUTE ===
PC=0: PlayAnim(Self, "CastFireball")
      → 애니메이션 요청 로그
      → PC=1, Status=Running

PC=1: YieldSeconds(100)  // 1.0초 = 100 데시밀리초
      → EventWait = {Type=Timer, RemainingTime=1.0}
      → Status = WaitingEvent
      → 실행 중단

Frame 21~30: (1초 경과, Timer 업데이트)
└─ RemainingTime 감소 → 0에 도달 → Status = Ready

=== Frame 30: EXECUTE 재개 ===
PC=2: SpawnEntity("/Game/BP_Fireball")
      → Stash.AllocateEntity() → EntityId=27
      → Registers[Spawned] = 27
      → PC=3

PC=3~5: GetPosition(R0, Self) → SetPosition(Spawned, R0) → MoveForward(Spawned, 500)
        → 파이어볼 위치 설정 및 이동 시작
        → PC=6

PC=6: WaitCollision(Spawned)
      → EventWait = {Type=Collision, WatchedEntity=27}
      → Status = WaitingEvent
      → 실행 중단

Frame 35: 외부에서 충돌 감지
└─ NotifyCollision(WatchedEntity=27, HitEntity=15)
   → Registers[Hit] = 15
   → Status = Ready

=== Frame 35: EXECUTE 재개 ===
PC=7: GetPosition(R3, Spawned)
PC=8: DestroyEntity(Spawned)
      → Stash.FreeEntity(27)
PC=9: ApplyDamageConst(Hit, 100)
      → Health[15] -= 100 (방어력 계산)
PC=10~15: VFX, ForEachInRadius, AoE 데미지...
PC=16: Halt
       → Status = Completed

=== PHASE 3: CLEANUP ===
├─ ApplyStoreChanges(Handle=5)
│     → 모든 PendingWrites를 Stash에 적용
│     → Entity 15: Health 갱신
│     → 주변 적들: Health 갱신, Burn 이펙트
├─ FinalizeVM(Handle=5)
│     → RuntimePool.Free(5)
│     → Generation++ (이전 핸들 무효화)
└─ CompletedVMs.Clear()
```

---

## 10. 확장 가이드

### 새 OpCode 추가

**1. EOpCode에 추가 (HktVMTypes.h)**
```cpp
enum class EOpCode : uint8
{
    // ... 기존 ...
    MyCustomOp = 70,  // 새 OpCode
};
```

**2. 핸들러 선언 (HktVMInterpreter.h)**
```cpp
class FHktVMInterpreter
{
    // ...
    void Op_MyCustomOp(FHktVMRuntime& Runtime, ...);
};
```

**3. 핸들러 구현 (HktVMInterpreterActions.cpp)**
```cpp
void FHktVMInterpreter::Op_MyCustomOp(FHktVMRuntime& Runtime,
    RegisterIndex Dst, RegisterIndex Src, int32 Param)
{
    int32 Value = Runtime.GetReg(Src);
    // ... 로직 ...
    Runtime.SetReg(Dst, Result);
}
```

**4. ExecuteInstruction에 케이스 추가 (HktVMInterpreter.cpp)**
```cpp
case EOpCode::MyCustomOp:
    Op_MyCustomOp(Runtime, Inst.Dst, Inst.Src1, Inst.GetSignedImm12());
    break;
```

**5. FlowBuilder에 메서드 추가 (HktVMProgram.h)**
```cpp
FFlowBuilder& MyCustomOp(RegisterIndex Dst, RegisterIndex Src, int32 Param)
{
    Emit(FInstruction::Make(EOpCode::MyCustomOp, Dst, Src, 0, Param));
    return *this;
}
```

### 새 PropertyId 추가

```cpp
namespace PropertyId
{
    // ... 기존 ...
    constexpr uint16 MyNewProperty = 60;
}
```

사용:
```cpp
Flow(TEXT("MyFlow"))
    .LoadStore(Reg::R0, PropertyId::MyNewProperty)
    .AddImm(Reg::R0, Reg::R0, 10)
    .SaveStore(PropertyId::MyNewProperty, Reg::R0)
    .Halt()
    .BuildAndRegister();
```

---

## 성능 특성

| 항목 | 값 |
|------|-----|
| 최대 VM 수/프레임 | 256 |
| 최대 명령어/VM/틱 | 10,000 |
| 레지스터 수 | 16 (32비트) |
| 최대 엔티티 | 1,024 |
| 최대 속성/엔티티 | 128 |
| 명령어 크기 | 4 바이트 |
| 핸들 오버헤드 | 4 바이트 (제너레이셔널) |
| FindInRadius | O(n) 선형 검색 |
| Stash 할당 | O(1) (FreeList) |

---

