# HktPhysics - VM 통합 가이드

## 개요

이 문서는 HktPhysics 모듈을 기존 VM 시스템과 통합하는 방법을 설명합니다.

---

## 1. 핵심 설계

### 역할 분리

```
┌────────────────────────────────────────────────────────────────┐
│ IHktTickable (인터페이스)                                      │
│   - Tick(DeltaSeconds)                                        │
│   - GetTickPriority()                                         │
│   - IsTickEnabled()                                           │
└────────────────────────────────────────────────────────────────┘
                            △
                            │ 구현
                            │
┌────────────────────────────────────────────────────────────────┐
│ FHktPhysicsWorld                                               │
│   - Tick() → 충돌 감지                                        │
│   - SetOnCollisionDetected() → 콜백 등록                      │
│   - AddWatchedEntity() ← VM에서 직접 호출                     │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ FHktVMProcessor                                                │
│   - TArray<IHktTickable*> Tickables                           │
│   - Tick() → Tickables 순회                                   │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ FHktVMInterpreter                                              │
│   - WaitCollision 시 PhysicsWorld->AddWatchedEntity()         │
│   - OnPhysicsCollision() 콜백으로 NotifyCollision 처리        │
└────────────────────────────────────────────────────────────────┘
```

### 데이터 흐름

```
[Flow 정의]
    │ SetColliderSphere(Spawned, 30, Layer::Projectile, Layer::Enemy)
    ▼
[Stash Property 저장]
    │ ColliderType, Radius, Layer, Mask
    ▼
[WaitCollision 실행]
    │ VMInterpreter → PhysicsWorld.AddWatchedEntity()
    ▼
[매 프레임]
    │ VMProcessor.Tick()
    │   → Build() → Execute()
    │   → for (Tickable : Tickables) Tickable->Tick()
    │       → PhysicsWorld.Tick() → DetectCollisions()
    ▼
[충돌 발생]
    │ PhysicsWorld → OnCollisionDetected 콜백
    │   → VMInterpreter.OnPhysicsCollision(WatchedEntity, HitEntity)
    ▼
[VM 재개]
    │ Hit 레지스터에 HitEntity 저장
    │ WaitCollision 이후 명령 실행
```

---

## 2. 파일 구조

```
HktCore/
├── Private/
│   └── Physics/
│       ├── HktPhysicsTypes.h      # 타입, 상수, PropertyId
│       ├── HktPhysicsMath.h/cpp   # 벡터/선분/레이 연산
│       ├── HktCollisionTests.h/cpp # 충돌 테스트 함수
│       ├── HktPhysicsWorld.h/cpp  # 메인 물리 월드 (IHktTickable 구현)
│       ├── HktPhysicsFactory.cpp  # 팩토리 함수
│       └── HktPhysicsFlowExtensions.h # Flow 예제
├── Public/
│   ├── HktPhysics.h               # 통합 헤더
│   ├── HktTickable.h              # IHktTickable 인터페이스
│   └── HktFlowBuilderPhysics.h    # FlowBuilder 확장
```

---

## 3. HktVMProcessor 수정

### 3.1 Tickable 관리 추가

```cpp
// HktVMProcessor.h
#include "HktTickable.h"

class FHktVMProcessor
{
public:
    /** Tickable 등록 (Priority 순 정렬) */
    void RegisterTickable(IHktTickable* Tickable);
    
    /** Tickable 해제 */
    void UnregisterTickable(IHktTickable* Tickable);
    
private:
    /** 등록된 Tickables (Priority 낮은 순) */
    TArray<IHktTickable*> Tickables;
    
    /** Priority 순 정렬 */
    void SortTickables();
};
```

### 3.2 RegisterTickable 구현

```cpp
// HktVMProcessor.cpp
void FHktVMProcessor::RegisterTickable(IHktTickable* Tickable)
{
    if (Tickable && !Tickables.Contains(Tickable))
    {
        Tickables.Add(Tickable);
        SortTickables();
    }
}

void FHktVMProcessor::UnregisterTickable(IHktTickable* Tickable)
{
    Tickables.Remove(Tickable);
}

void FHktVMProcessor::SortTickables()
{
    Tickables.Sort([](const IHktTickable& A, const IHktTickable& B)
    {
        return A.GetTickPriority() < B.GetTickPriority();
    });
}
```

### 3.3 Tick에서 Tickables 호출

```cpp
void FHktVMProcessor::Tick(int32 CurrentFrame, float DeltaSeconds)
{
    Build(CurrentFrame);
    Execute(DeltaSeconds);
    
    // Tickables 실행 (Physics, Navigation 등)
    for (IHktTickable* Tickable : Tickables)
    {
        if (Tickable->IsTickEnabled())
        {
            Tickable->Tick(DeltaSeconds);
        }
    }
    
    Cleanup(CurrentFrame);
}
```

---

## 4. HktVMInterpreter 수정

### 4.1 PhysicsWorld 참조 및 콜백 설정

```cpp
// HktVMInterpreter.h
class FHktPhysicsWorld;

class FHktVMInterpreter
{
public:
    /** PhysicsWorld 연결 */
    void SetPhysicsWorld(FHktPhysicsWorld* InPhysicsWorld);
    
    /** Physics 충돌 콜백 (PhysicsWorld에서 호출) */
    void OnPhysicsCollision(FHktEntityId WatchedEntity, FHktEntityId HitEntity);
    
private:
    FHktPhysicsWorld* PhysicsWorld = nullptr;
};
```

### 4.2 SetPhysicsWorld 구현

```cpp
// HktVMInterpreter.cpp
void FHktVMInterpreter::SetPhysicsWorld(FHktPhysicsWorld* InPhysicsWorld)
{
    PhysicsWorld = InPhysicsWorld;
    
    if (PhysicsWorld)
    {
        PhysicsWorld->SetOnCollisionDetected(
            FOnCollisionDetected::CreateRaw(this, &FHktVMInterpreter::OnPhysicsCollision));
    }
}
```

### 4.3 WaitCollision에서 Watch 등록

```cpp
EVMStatus FHktVMInterpreter::Op_WaitCollision(FHktVMRuntime& Runtime, RegisterIndex WatchEntity)
{
    FHktEntityId WatchedEntity = Runtime.GetRegEntity(WatchEntity);
    
    Runtime.EventWait.Type = EWaitEventType::Collision;
    Runtime.EventWait.WatchedEntity = WatchedEntity;
    
    // PhysicsWorld에 Watch 등록
    if (PhysicsWorld)
    {
        PhysicsWorld->AddWatchedEntity(WatchedEntity);
    }
    
    return EVMStatus::WaitingEvent;
}
```

### 4.4 OnPhysicsCollision 콜백 구현

```cpp
void FHktVMInterpreter::OnPhysicsCollision(FHktEntityId WatchedEntity, FHktEntityId HitEntity)
{
    // WatchedEntity를 가진 Runtime 찾기
    for (auto& [RuntimeId, Runtime] : Runtimes)
    {
        if (Runtime.Status == EVMStatus::WaitingEvent &&
            Runtime.EventWait.Type == EWaitEventType::Collision &&
            Runtime.EventWait.WatchedEntity == WatchedEntity)
        {
            // Watch 해제
            if (PhysicsWorld)
            {
                PhysicsWorld->RemoveWatchedEntity(WatchedEntity);
            }
            
            // Hit 레지스터에 결과 저장
            Runtime.SetRegEntity(Reg::Hit, HitEntity);
            Runtime.EventWait.Reset();
            Runtime.Status = EVMStatus::Ready;
            
            break;
        }
    }
}
```

---

## 5. 초기화 코드 예시

```cpp
// GameInstance 또는 GameMode에서
void UMyGameInstance::Init()
{
    // Stash 생성
    MasterStash = MakeUnique<FHktStash>();
    MasterStash->Initialize();
    
    // PhysicsWorld 생성
    PhysicsWorld = CreatePhysicsWorld(MasterStash.Get());
    
    // VMProcessor 생성
    VMProcessor = MakeUnique<FHktVMProcessor>();
    VMProcessor->Initialize(MasterStash.Get());
    
    // Interpreter에 PhysicsWorld 연결
    VMProcessor->GetInterpreter()->SetPhysicsWorld(PhysicsWorld.Get());
    
    // PhysicsWorld를 Tickable로 등록
    VMProcessor->RegisterTickable(PhysicsWorld.Get());
    
    // Physics Properties 등록
    RegisterPhysicsProperties();
}

void UMyGameInstance::Tick(float DeltaSeconds)
{
    // VMProcessor.Tick()이 PhysicsWorld.Tick()도 호출
    VMProcessor->Tick(GFrameCounter, DeltaSeconds);
}
```

---

## 6. Flow 정의 예제

### 6.1 파이어볼 (Physics 버전)

```cpp
#include "HktFlowBuilderPhysics.h"

void RegisterFireball_Physics()
{
    using namespace Reg;
    using namespace HktPhysics;
    
    auto Builder = Flow(TEXT("Ability.Skill.Fireball.Physics"));
    
    Builder
        .Log(TEXT("Fireball: 시전"))
        .PlayAnim(Self, TEXT("Cast"))
        .WaitSeconds(0.5f)
        .SpawnEntity(TEXT("/Game/BP_Fireball"));
    
    // 충돌체 설정 (헬퍼 함수 사용)
    SetColliderSphere(Builder, Spawned, 30, Layer::Projectile, Layer::Enemy);
    
    Builder
        .GetPosition(R0, Self)
        .SetPosition(Spawned, R0)
        .MoveForward(Spawned, 1000)
        .WaitCollision(Spawned)
        
        // 충돌 처리
        .GetPosition(R3, Spawned)
        .DestroyEntity(Spawned)
        .ApplyDamageConst(Hit, 100)
        
        .ForEachInRadius(Hit, 300)
            .Move(Target, Iter)
            .ApplyDamageConst(Target, 50)
        .EndForEach()
        
        .Halt()
        .BuildAndRegister();
}
```

### 6.2 캐릭터 스폰 (Capsule 충돌체)

```cpp
void RegisterCharacterSpawn_Physics()
{
    using namespace Reg;
    using namespace HktPhysics;
    
    auto Builder = Flow(TEXT("Event.Character.Spawn.Physics"));
    
    Builder
        .SpawnEntity(TEXT("/Game/BP_Character"))
        .Move(Self, Spawned);
    
    // Capsule 충돌체 설정 (HalfHeight=90cm, Radius=40cm)
    SetColliderCapsule(Builder, Self, 90, 40, Layer::Player, Layer::All);
    
    Builder
        .LoadStore(R0, PropertyId::TargetPosX)
        .LoadStore(R1, PropertyId::TargetPosY)
        .LoadStore(R2, PropertyId::TargetPosZ)
        .SetPosition(Self, R0)
        .PlayAnim(Self, TEXT("Idle"))
        .Halt()
        .BuildAndRegister();
}
```

---

## 7. 데이터 흐름

```
┌─────────────────────────────────────────────────────────────┐
│ Flow에서 SetColliderSphere/Capsule 호출                     │
│ → Store에 ColliderType, Radius, Layer, Mask 저장           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ VMProcessor::Cleanup → Stash에 Property 적용                │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ WaitCollision 실행 → PhysicsWorld.AddWatchedEntity()       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 매 프레임: PhysicsWorld.DetectCollisions()                  │
│ → WatchedEntities 순회                                      │
│ → Stash에서 위치/충돌체 정보 읽기                           │
│ → 충돌 테스트                                               │
│ → 충돌 시 VMProcessor.NotifyCollision() 호출               │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ NotifyCollision → Hit 레지스터 설정 → VM 재개               │
└─────────────────────────────────────────────────────────────┘
```

---

## 8. 레이어 설정 가이드

| 레이어 | 값 | 용도 |
|--------|-----|------|
| None | 0x00 | 충돌 비활성화 |
| Default | 0x01 | 기본 |
| Player | 0x02 | 플레이어 캐릭터 |
| Enemy | 0x04 | 적 캐릭터/유닛 |
| Projectile | 0x08 | 투사체 |
| Trigger | 0x10 | 트리거 볼륨 |
| Environment | 0x20 | 환경 오브젝트 |
| All | 0xFF | 모두 |

### 일반적인 조합

```cpp
// 플레이어 발사체: Projectile 레이어, Enemy만 충돌
SetColliderSphere(Builder, Spawned, 30, Layer::Projectile, Layer::Enemy);

// 적 발사체: Projectile 레이어, Player만 충돌
SetColliderSphere(Builder, Spawned, 30, Layer::Projectile, Layer::Player);

// 플레이어 캐릭터: Player 레이어, 모든 것과 충돌
SetColliderCapsule(Builder, Self, 90, 40, Layer::Player, Layer::All);

// 트리거 영역: Trigger 레이어, Characters만 충돌
SetColliderSphere(Builder, Self, 200, Layer::Trigger, Layer::Player | Layer::Enemy);
```

---

## 9. 성능 고려사항

| 항목 | 현재 구현 | 최적화 가능 |
|------|----------|------------|
| Broad Phase | O(W×N) 브루트포스 | Grid/Octree 적용 |
| 활성 충돌체 캐시 | 지연 갱신 | 이벤트 기반 갱신 |
| Stash 읽기 | 직접 읽기 | 로컬 캐시 |
| Capsule-Capsule | 정확한 해석해 | 간소화 가능 |

W = Watched 엔티티 수 (보통 < 10)
N = 활성 충돌체 수 (보통 < 100)

현재 구현은 수백 개 충돌체까지 충분히 빠릅니다.

---

## 10. 테스트 체크리스트

- [ ] Sphere-Sphere 충돌 감지
- [ ] Sphere-Capsule 충돌 감지
- [ ] Capsule-Capsule 충돌 감지
- [ ] WaitCollision → NotifyCollision 흐름
- [ ] 레이어/마스크 필터링
- [ ] OverlapSphere 쿼리
- [ ] Raycast 쿼리
- [ ] SweepSphere 쿼리
- [ ] 엔티티 생성/삭제 시 ActiveColliders 갱신
