# HktStory Skill — Story 작성 가이드

HktStory는 register-based bytecode VM 위에서 동작하는 게임 로직 Flow를 C++ fluent builder DSL로 작성하는 시스템이다.

## Architecture Overview

```
Story(.cpp) → FHktStoryBuilder → FHktVMProgram(bytecode) → FHktVMInterpreter(실행)
```

- **서버 권위적**: 클라이언트는 Precondition으로 사전 판단만 하고, 서버가 바이트코드를 실행하여 최종 검증
- **Pure C++**: UObject/UWorld 의존 없음. HktCore 모듈은 순수 C++ VM
- **자가 등록**: `HKT_REGISTER_STORY_BODY()` 매크로로 각 .cpp가 자동 등록. 중앙 헤더 수정 불필요

## File Structure

- 파일 위치: `Source/HktStory/Private/Definitions/HktStory<Name>.cpp`
- 네임스페이스: `namespace HktStory<Name> { ... }`
- 파일명 규칙: `HktStory` + PascalCase 이름 (예: `HktStoryFireball.cpp`)

## Template — 새 Story 작성 패턴

```cpp
// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "HktCoreProperties.h"
#include "HktStoryRegistry.h"
#include "NativeGameplayTags.h"

namespace HktStory<Name>
{
    // Story Name — "Story.Event.<Category>.<Name>" 또는 "Story.Flow.<Category>.<Name>"
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Story_<Name>, "Story.Event.<Category>.<Name>", "<설명>");

    // 필요한 태그 정의 (Entity, Anim, VFX, Sound, Effect 등)
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tag_..., "...", "...");

    /**
     * ================================================================
     * <Story 이름> Flow
     *
     * 자연어로 읽으면:
     * "<이 Flow가 하는 일을 자연어로 기술>"
     *
     * Self = <소스 엔티티 설명>, Target = <타겟 엔티티 설명>
     * ================================================================
     */
    HKT_REGISTER_STORY_BODY()
    {
        using namespace Reg;

        Story(Story_<Name>)
            // ... builder chain ...
            .Halt()
            .BuildAndRegister();
    }
}
```

## Tag 네이밍 규칙

| 카테고리 | 패턴 | 예시 |
|---------|------|------|
| Story (스킬) | `Story.Event.Skill.<Name>` | `Story.Event.Skill.Fireball` |
| Story (공격) | `Story.Event.Attack.<Name>` | `Story.Event.Attack.Basic` |
| Story (이동) | `Story.Event.Move.<Name>` | `Story.Event.Move.ToLocation` |
| Story (아이템) | `Story.Event.Item.<Name>` | `Story.Event.Item.Pickup` |
| Story (스포너) | `Story.Flow.Spawner.<Name>` | `Story.Flow.Spawner.DungeonEntrance` |
| Entity | `Entity.<Type>.<Name>` | `Entity.Projectile.Fireball` |
| Animation | `Anim.<Part>.<Action>.<Name>` | `Anim.UpperBody.Cast.Fireball` |
| VFX | `VFX.Niagara.<Name>` | `VFX.Niagara.DirectHit` |
| Sound | `Sound.<Name>` | `Sound.Explosion` |
| Effect | `Effect.<Name>` | `Effect.Burn` |

## Register System (16개)

| Register | 별칭 | 용도 |
|----------|------|------|
| R0-R8 | `R0`~`R8` | 범용. 자유롭게 사용 |
| R9 | `Temp` | Builder 내부 헬퍼 전용 (SaveConst, ApplyDamage 등). **직접 사용 금지** |
| R10 | `Self` | Event.SourceEntity — 항상 유효 |
| R11 | `Target` | Event.TargetEntity — 항상 유효 |
| R12 | `Spawned` | `SpawnEntity()` 호출 후 유효 |
| R13 | `Hit` | `WaitCollision()` 후 유효 |
| R14 | `Iter` | `NextFound()` / ForEach 루프 내 유효 |
| R15 | `Flag` / `Count` | 비교 결과, 카운트 결과 저장 |

**주의**: `ApplyDamage`/`ApplyDamageConst`는 내부적으로 R7, R8, R9(Temp)를 사용한다. 이 연산 전후로 R7-R8에 중요한 값을 보관하지 말 것.

## Builder API Reference

### Story Policy

```cpp
.CancelOnDuplicate()                    // 같은 엔티티에 동일 이벤트 재발생 시 기존 VM 취소 (예: MoveTo)
.SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool { ... })
```

### Control Flow

```cpp
.Label(TEXT("name"))                     // 라벨 정의 (점프 대상)
.Jump(TEXT("name"))                      // 무조건 점프
.JumpIf(Cond, TEXT("name"))             // Cond != 0 이면 점프
.JumpIfNot(Cond, TEXT("name"))          // Cond == 0 이면 점프
.Yield(Frames)                           // N 프레임 대기
.WaitSeconds(1.0f)                       // N초 대기
.Halt()                                  // 프로그램 종료 (성공)
.Fail()                                  // 프로그램 종료 (실패 — 검증 위반)
```

### Event Wait

```cpp
.WaitCollision(Entity)                   // 충돌 대기 → Hit 레지스터에 대상 저장. 기본값: Spawned
.WaitAnimEnd(Entity)                     // 애니메이션 종료 대기 (현재 Yield(1)로 구현)
.WaitMoveEnd(Entity)                     // 이동 완료 대기
```

### Data Operations

```cpp
.LoadConst(Dst, Value)                   // Dst = 상수값
.LoadStore(Dst, PropertyId::X)           // Dst = Self의 프로퍼티 X
.LoadStoreEntity(Dst, Entity, PropertyId::X)  // Dst = Entity의 프로퍼티 X
.LoadEntityProperty(Dst, Entity, PropertyId::X)  // LoadStoreEntity 별칭
.SaveStore(PropertyId::X, Src)           // Self의 프로퍼티 X = Src
.SaveStoreEntity(Entity, PropertyId::X, Src)   // Entity의 프로퍼티 X = Src
.SaveEntityProperty(Entity, PropertyId::X, Src)  // SaveStoreEntity 별칭
.SaveConst(PropertyId::X, Value)         // Self의 프로퍼티 X = 상수 (Temp 사용)
.SaveConstEntity(Entity, PropertyId::X, Value)  // Entity의 프로퍼티 X = 상수 (Temp 사용)
.Move(Dst, Src)                          // Dst = Src (레지스터 복사)
```

### Arithmetic / Comparison

```cpp
.Add(Dst, Src1, Src2)                   // Dst = Src1 + Src2
.Sub(Dst, Src1, Src2)                   // Dst = Src1 - Src2
.Mul(Dst, Src1, Src2)                   // Dst = Src1 * Src2
.Div(Dst, Src1, Src2)                   // Dst = Src1 / Src2
.AddImm(Dst, Src, Imm)                  // Dst = Src + Imm (즉시값)

.CmpEq(Dst, Src1, Src2)                // Dst = (Src1 == Src2) ? 1 : 0
.CmpNe(Dst, Src1, Src2)                // Dst = (Src1 != Src2) ? 1 : 0
.CmpLt(Dst, Src1, Src2)                // Dst = (Src1 <  Src2) ? 1 : 0
.CmpLe(Dst, Src1, Src2)                // Dst = (Src1 <= Src2) ? 1 : 0
.CmpGt(Dst, Src1, Src2)                // Dst = (Src1 >  Src2) ? 1 : 0
.CmpGe(Dst, Src1, Src2)                // Dst = (Src1 >= Src2) ? 1 : 0
```

### Entity Management

```cpp
.SpawnEntity(EntityClassTag)             // 엔티티 생성 → Spawned 레지스터에 저장
.DestroyEntity(Entity)                   // 엔티티 제거
```

### Position & Movement

```cpp
.GetPosition(DstBase, Entity)            // DstBase, DstBase+1, DstBase+2 = PosX, PosY, PosZ
.SetPosition(Entity, SrcBase)            // Entity 위치 = SrcBase, SrcBase+1, SrcBase+2
.MoveToward(Entity, TargetPosBase, Force) // 목표 위치로 이동 시작 (F=ma)
.MoveForward(Entity, Force)              // 전방으로 이동 (투사체용)
.StopMovement(Entity)                    // 이동 중지, 속도 0
.GetDistance(Dst, Entity1, Entity2)      // Dst = 두 엔티티 간 거리 (cm)
```

**Position 규칙**: 위치는 항상 3개 연속 레지스터를 사용한다 (X, Y, Z). `GetPosition(R0, Self)`는 R0=X, R1=Y, R2=Z. 다른 위치를 저장하려면 `R3`부터 사용.

### Spatial Query & ForEach

```cpp
// 방법 1: 수동 루프
.FindInRadius(CenterEntity, RadiusCm)    // 범위 내 검색 시작 → Count에 결과 수
.NextFound()                             // 다음 결과 → Iter, 끝이면 Flag=0

// 방법 2: ForEach 편의 매크로 (내부적으로 FindInRadius + Label + NextFound + Jump 생성)
.ForEachInRadius(CenterEntity, RadiusCm)
    .Move(Target, Iter)                  // 순회 대상을 Target에 복사
    .ApplyDamageConst(Target, 50)
.EndForEach()
```

### Combat

```cpp
.ApplyDamage(Target, AmountReg)          // 피해 적용: ActualDmg = Max(1, Amount - Defense), Health -= ActualDmg
.ApplyDamageConst(Target, Amount)        // 상수 피해 적용 (내부: LoadConst + ApplyDamage)
```

**주의**: R7, R8, Temp(R9)를 내부적으로 사용함.

### Tags

```cpp
.AddTag(Entity, Tag)                     // 엔티티에 태그 추가
.RemoveTag(Entity, Tag)                  // 엔티티에서 태그 제거
.HasTag(Dst, Entity, Tag)                // Dst = Entity가 Tag 보유 ? 1 : 0
```

### Presentation (클라이언트 렌더링 힌트)

```cpp
.ApplyEffect(Target, EffectTag)          // 버프/디버프 이펙트 적용
.RemoveEffect(Target, EffectTag)         // 이펙트 제거
.PlayVFX(PosBase, VFXTag)               // 위치에 VFX 재생 (PosBase = 3연속 레지스터)
.PlayVFXAttached(Entity, VFXTag)        // 엔티티에 부착된 VFX 재생
.PlaySound(SoundTag)                     // 사운드 재생
.PlaySoundAtLocation(PosBase, SoundTag) // 위치에 사운드 재생
```

### World Query

```cpp
.CountByTag(Dst, Tag)                    // Dst = 해당 태그 보유 엔티티 수
.GetWorldTime(Dst)                       // Dst = 현재 프레임 번호
.RandomInt(Dst, ModulusReg)             // Dst = 결정론적 랜덤 [0, ModulusReg)
.HasPlayerInGroup(Dst)                   // Dst = relevancy group 내 플레이어 존재 ? 1 : 0
```

### Item System

```cpp
.CountByOwner(Dst, OwnerEntity, Tag)     // Dst = Owner가 소유한 Tag 매칭 엔티티 수
.FindByOwner(OwnerEntity, Tag)           // Owner 소유 Tag 매칭 엔티티 검색 → NextFound()로 순회
.SetOwnerUid(Entity)                     // 현재 PlayerUid를 Entity의 OwnerUid로 설정
.ClearOwnerUid(Entity)                   // Entity의 OwnerUid 초기화 (무주물)
```

### Stance & Utility

```cpp
.SetStance(Entity, StanceTag)            // Stance 태그 설정
.Log(TEXT("message"))                    // 디버그 로그 출력
```

## PropertyId 목록

### Hot Properties (O(1) 직접 인덱싱)

| ID | 용도 |
|----|------|
| `PosX`, `PosY`, `PosZ` | 위치 |
| `RotYaw` | 회전 |
| `MoveTargetX/Y/Z` | 이동 목표 |
| `MoveForce` | 이동 힘 |
| `IsMoving` | 이동 중 여부 |
| `MaxSpeed` | 최대 이동 속도 |
| `Health`, `MaxHealth` | 체력 |
| `AttackPower` | 공격력 |
| `Defense` | 방어력 |
| `Team` | 팀 |
| `Mana`, `MaxMana` | 마나 |
| `OwnerEntity` | 소유 엔티티 |
| `EntitySpawnTag` | 스폰 태그 |
| `Stance` | 스탠스 |

### Cold Properties (페어 배열 순회)

| ID | 용도 |
|----|------|
| `TargetPosX/Y/Z` | 이벤트 파라미터: 이동 목표 |
| `Param0`~`Param3` | 이벤트 파라미터: 범용 |
| `AnimState`, `AnimStateUpper`, `VisualState` | 애니메이션/비주얼 |
| `VelX/Y/Z` | 속도 |
| `Mass` | 질량 |
| `CollisionRadius` | 충돌 반경 |
| `ItemState` | 아이템 상태 (0=Ground, 1=InBag, 2=Active) |
| `ItemId` | 아이템 ID |
| `BagSlot` | 가방 슬롯 번호 |
| `ActionSlot` | 액션 슬롯 (-1=없음) |
| `BagCapacity` | 가방 용량 |
| `IsNPC` | NPC 여부 |
| `SpawnFlowTag` | 스폰 Flow 태그 |

## Story 패턴별 예시

### Pattern 1: 단순 스킬 (순차 실행)

```cpp
// 기본 공격 — 애니메이션 재생 후 피해 적용
Story(Story_BasicAttack)
    .AddTag(Self, Tag_Anim_Attack)
    .WaitAnimEnd(Self)
    .LoadStore(R0, PropertyId::AttackPower)
    .ApplyDamage(Target, R0)
    .PlayVFXAttached(Target, VFX_HitSpark)
    .RemoveTag(Self, Tag_Anim_Attack)
    .Halt()
    .BuildAndRegister();
```

### Pattern 2: 투사체 스킬 (Spawn + Collision)

```cpp
// 파이어볼 — 생성, 발사, 충돌 시 폭발
Story(Story_Fireball)
    .AddTag(Self, Tag_Cast)
    .WaitSeconds(1.0f)
    .SpawnEntity(Entity_Projectile)
    .GetPosition(R0, Self)
    .SetPosition(Spawned, R0)
    .MoveForward(Spawned, 500)
    .WaitCollision(Spawned)              // Hit = 충돌 대상
    .GetPosition(R3, Spawned)            // R3-R5 = 폭발 위치 저장
    .DestroyEntity(Spawned)
    .ApplyDamageConst(Hit, 100)
    .ForEachInRadius(Hit, 300)
        .Move(Target, Iter)
        .ApplyDamageConst(Target, 50)
        .ApplyEffect(Target, Effect_Burn)
    .EndForEach()
    .RemoveTag(Self, Tag_Cast)
    .Halt()
    .BuildAndRegister();
```

### Pattern 3: 조건 분기 + 실패 처리

```cpp
// 아이템 드랍 — 소유자 확인 후 실행
Story(Event_Item_Drop)
    .SetPrecondition([](const FHktWorldState& WS, const FHktEvent& E) -> bool {
        return WS.IsValidEntity(E.SourceEntity) && WS.IsValidEntity(E.TargetEntity)
            && WS.GetProperty(E.TargetEntity, PropertyId::OwnerEntity) == E.SourceEntity;
    })
    .LoadEntityProperty(R0, Target, PropertyId::OwnerEntity)
    .CmpNe(Flag, R0, Self)
    .JumpIf(Flag, TEXT("fail"))
    // ... 로직 ...
    .Halt()
.Label(TEXT("fail"))
    .Log(TEXT("Drop failed"))
    .Fail()
.BuildAndRegister();
```

### Pattern 4: 루핑 스포너 (무한 루프 + 조건)

```cpp
// 근접 스포너 — 5초마다 체크, 조건 충족 시 NPC 생성
Story(Story_Spawner)
.Label(TEXT("check"))
    .HasPlayerInGroup(Flag)
    .JumpIfNot(Flag, TEXT("sleep"))
    .CountByTag(R0, Entity_NPC_Skeleton)
    .LoadConst(R1, 3)
    .CmpGe(Flag, R0, R1)
    .JumpIf(Flag, TEXT("sleep"))
    // NPC 생성
    .SpawnEntity(Entity_NPC_Skeleton)
    .SaveConstEntity(Spawned, PropertyId::Health, 60)
    .SaveConstEntity(Spawned, PropertyId::MaxHealth, 60)
    .GetPosition(R3, Self)
    .SetPosition(Spawned, R3)
.Label(TEXT("sleep"))
    .WaitSeconds(5.0f)
    .Jump(TEXT("check"))
.BuildAndRegister();
```

### Pattern 5: 이동 (CancelOnDuplicate)

```cpp
// 이동 — 중복 발생 시 이전 이동 취소
Story(Story_MoveTo)
    .CancelOnDuplicate()
    .LoadStore(R0, PropertyId::TargetPosX)
    .LoadStore(R1, PropertyId::TargetPosY)
    .LoadStore(R2, PropertyId::TargetPosZ)
    .MoveToward(Self, R0, 150)
    .WaitMoveEnd(Self)
    .StopMovement(Self)
    .Halt()
    .BuildAndRegister();
```

### Pattern 6: 회복 (Clamp 로직)

```cpp
// 힐 — 체력을 회복하되 최대치 초과 방지
Story(Story_Heal)
    .AddTag(Self, Tag_Cast_Heal)
    .WaitSeconds(0.8f)
    .LoadStore(R0, PropertyId::Health)
    .LoadStore(R1, PropertyId::MaxHealth)
    .LoadConst(R2, 50)                   // 회복량
    .Add(R0, R0, R2)                     // 새 체력
    .CmpGt(R3, R0, R1)                   // 최대 초과?
    .JumpIfNot(R3, TEXT("NoClamp"))
    .Move(R0, R1)                        // 최대로 제한
.Label(TEXT("NoClamp"))
    .SaveStore(PropertyId::Health, R0)
    .RemoveTag(Self, Tag_Cast_Heal)
    .Halt()
    .BuildAndRegister();
```

## Build-Time Validation

Builder의 `Build()`/`BuildAndRegister()`는 다음을 자동 수행:
1. `Halt()` 누락 시 자동 추가
2. Label fixup 해결 (점프 대상 주소 패칭)
3. **Entity Flow Validation**: 특수 레지스터 초기화 순서 검증
   - `Self`, `Target`: 항상 유효 (Event에서 초기화)
   - `Spawned`: `SpawnEntity()` 이후 유효
   - `Hit`: `WaitCollision()` 이후 유효
   - `Iter`: `NextFound()` 이후 유효
   - 초기화 전 사용 시 빌드 실패 → 등록되지 않음

## Checklist — 새 Story 작성 시

1. `Source/HktStory/Private/Definitions/HktStory<Name>.cpp` 파일 생성
2. 고유 namespace로 감싸기: `namespace HktStory<Name> { ... }`
3. `UE_DEFINE_GAMEPLAY_TAG_COMMENT`로 Story 태그 및 필요한 태그 정의
4. 자연어 주석 블록 작성 (Flow의 동작을 자연어로 설명)
5. `HKT_REGISTER_STORY_BODY()` 내에서 `using namespace Reg;` 선언
6. `Story(Tag)` → builder chain → `.Halt()` → `.BuildAndRegister()` 패턴 사용
7. 조건 검증이 필요하면 `.SetPrecondition(lambda)` + 바이트코드 내 검증 (이중 검증)
8. 실패 경로는 `.Label(TEXT("fail"))` → `.Fail()`로 처리
9. R9(Temp)는 직접 사용하지 않기
10. `ApplyDamage` 사용 시 R7-R8에 중요값 보관하지 않기
11. Position 연산은 연속 3레지스터 (R0-R2 또는 R3-R5) 사용
12. 필요한 include 추가 (Precondition 사용 시 `HktWorldState.h`, `HktCoreEvents.h` 추가)
