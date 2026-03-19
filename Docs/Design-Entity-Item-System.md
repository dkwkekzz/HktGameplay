# Entity 생명주기 및 아이템 소유/활성화 정책 기획서

## 문서 개요

HktGameplay 모듈의 Entity 생명주기(Lifecycle)와 아이템 소유/활성화 정책을 정의하는 게임 기획서이다.
현재 코드베이스의 구현 상태를 기준으로 전체 설계를 정리하고, 미구현 영역과 개선 제안을 포함한다.

---

## 0. Tag 네이밍 컨벤션

모든 GameplayTag는 아래 규칙을 따른다.

### 0.1 Story Tags — `Story.{호출형태}.{카테고리}.{이름}`

Story(VM 프로그램)를 식별하는 태그. 접두사로 호출 출처를 구분한다.

| 접두사 | 호출 출처 | 설명 | 예시 |
|--------|-----------|------|------|
| `Story.Event.*` | Client Intent | 클라이언트가 최초로 fire | `Story.Event.Item.Pickup`, `Story.Event.Attack.Basic` |
| `Story.Flow.*` | Map Event | 서버가 자체적으로 fire | `Story.Flow.Spawner.GoblinCamp`, `Story.Flow.NPC.Lifecycle` |
| `Story.State.*` | 지속 상태 | 서버가 fire, 지속 유지 | `Story.State.Player.InWorld` |

### 0.2 Entity Tags — `Entity.*`

엔티티 관련 모든 태그의 루트.

| 패턴 | 용도 | 예시 |
|------|------|------|
| `Entity.{Type}.{Name}` | 엔티티 유형 (SpawnEntity의 ClassTag) | `Entity.Character.Player`, `Entity.Item.WoodenSword`, `Entity.NPC.Goblin` |
| `Entity.Attr.{Category}.{Name}` | 엔티티 속성 (AddTag로 부여하는 분류) | `Entity.Attr.Weapon.Sword`, `Entity.Attr.Item.Material`, `Entity.Attr.NPC.Hostile` |
| `Entity.{PropertyName}.{Value}` | Property 값으로 사용되는 태그 | `Entity.Stance.Spear`, `Entity.Stance.Sword1H` |

### 0.3 Presentation Tags

| 패턴 | 용도 | 예시 |
|------|------|------|
| `Anim.{Layer}.{Category}.{Name}` | 애니메이션 상태 태그 | `Anim.FullBody.Locomotion.Idle`, `Anim.UpperBody.Combat.Attack` |
| `VFX.{Name}` | VFX 식별자 | `VFX.SpawnEffect`, `VFX.HitSpark` |
| `Sound.{Name}` | 사운드 식별자 | `Sound.Spawn`, `Sound.Hit` |
| `Widget.{Name}` | UI 위젯 식별자 | `Widget.IngameHud` |

### 0.4 System Tags

| 패턴 | 용도 | 예시 |
|------|------|------|
| `Effect.{Name}` | 게임 이펙트 (버프/디버프) | `Effect.Burn` |

---

## 1. Event와 Story의 구조

### 1.1 근본 개념

**Story**는 **Event**로부터 실행되는 구조이다.
모든 Story는 GameplayTag로 지칭되며, 클라이언트 또는 서버가 최초로 시작(fire)하는 Tag가 **Event**가 된다.

구현적으로 Event와 Story는 동일하다 — 둘 다 Tag로 식별되는 VM 프로그램이다.
차이는 **개념적**이다: Event는 최초의 트리거(진입점)이고, Story는 그로부터 파생되는 후속 로직이다.

### 1.2 Event의 두 가지 출처

| 출처 | 이름 | 설명 | 예시 |
|------|------|------|------|
| **서버** | Map Event | 서버가 자체적으로 요청하는 자연적 작용 | `Story.Flow.Spawner.Item.TreeDrop`, `Story.Flow.NPC.Lifecycle`, `Story.State.Player.InWorld` |
| **클라이언트** | Client Intent | 사용자가 요청하는 상호작용 | `Story.Event.Item.Pickup`, `Story.Event.Item.Activate`, `Story.Event.Item.Drop` |

### 1.3 Event와 Story의 분리

핵심 구분: **Event는 최초의 트리거**이다. Story 내부에서 파생되는 로직은 Event가 아니다.

```
예시: 아이템 지급(Grant)

  Event(최초 트리거)          Story(파생 로직)
  ─────────────────          ──────────────────
  퀘스트 완료 UI 클릭    ──►  퀘스트 보상 처리 Story
                               └─► Grant (아이템 생성+InBag)
  NPC 대화 선택          ──►  NPC 상점 Story
                               └─► Grant (아이템 생성+InBag)
```

Grant 자체는 Event가 아니다. 최초의 트리거(퀘스트 UI, NPC 대화 등)가 Event이고,
Grant는 그 Event의 Story 내부에서 수행되는 아이템 생성+소유 로직이다.

반면 Pickup은 그 자체가 Event이다 — 클라이언트가 "이 아이템을 줍겠다"고 직접 요청한다.

### 1.4 Event 검증 (미구현 — Gap)

**최초의 Story(Event)는 검증이 필요하다.** 클라이언트가 fire하는 Client Intent는 서버에서 사전 조건을 검증해야 한다. 현재 이 검증 레이어가 빠져 있다.

검증이 필요한 이유:
- Client Intent는 클라이언트가 임의로 fire할 수 있다
- 검증 없이 Story가 실행되면 부정 행위가 가능하다
- Map Event는 서버 자체가 fire하므로 검증이 불필요하거나 최소화할 수 있다

검증의 위치:
- Story 내부의 조건 분기 (현재 방식: Pickup에서 거리/용량 검증)
- Event fire 시점의 사전 검증 레이어 (미구현: Story 실행 전에 걸러내는 게이트)

---

## 2. Entity 생명주기

### 2.1 핵심 개념

모든 게임 오브젝트(캐릭터, 아이템, NPC, 이펙트)는 **Entity**이다.
Entity는 순수 데이터이며, `FHktEntityId`(int32)로 식별된다.
Entity의 종류는 `FGameplayTagContainer`의 태그로 구분한다 (예: `Entity.Character.Player`, `Entity.Item.Sword`).

### 2.2 식별 체계

| 식별자 | 타입 | 범위 | 용도 |
|--------|------|------|------|
| `FHktEntityId` | int32 | 그룹 내 유일 | 런타임 엔티티 참조 (NextEntityId++ 순차 할당) |
| `PlayerUid` | int64 | 전역 유일 | 계정 식별, DB 키, 엔티티 소유권 |
| `OwnerEntity` | int32 (Hot Property) | 그룹 내 | 엔티티-엔티티 소유 관계 (아이템→캐릭터) |
| `EntitySpawnTag` | int32 (Hot Property) | GameplayTag 넷인덱스 | Presentation 시각 유형 |

### 2.3 소유 관계의 이중 구조

**계정 소유 (Account Ownership)** — `OwnerUid` (int64)
- Entity가 어떤 플레이어 계정에 속하는지 나타낸다.
- DB 저장/로드, 로그아웃 시 엔티티 추출의 기준.
- `Op_SpawnEntity`에서 `Runtime.PlayerUid != 0`일 때 자동 설정된다.

**엔티티 소유 (Entity Ownership)** — `OwnerEntity` (int32, Hot Property)
- 아이템이 어떤 캐릭터 엔티티에 소속되는지 나타낸다.
- Pickup/Activate/Drop 등 Story 검증의 기준.
- `Op_SpawnEntity`에서 `Reg::Self`로 자동 설정된다.

### 2.4 접속에서 월드 존재까지의 전체 흐름

```
Phase 1: 네트워크 접속 및 인증
  1. 클라이언트 접속 → UE5 GameMode.PostLogin() 호출
  2. PlayerController에 UHktWorldPlayerComponent 부착
  3. BeginPlay()에서 PlayerState.UniqueNetId 해시 → PlayerUid(int64) 산출
  4. IHktServerRule.OnEvent_GameModePostLogin(IHktWorldPlayer) 호출
       │
       ▼
Phase 2: DB 로드 (비동기)
  1. ServerRule이 CachedDB->LoadPlayerRecordAsync(PlayerUid) 호출
  2. FHktPlayerRecord 반환:
     - PlayerUid, LastLoginTime, LastPosition
     - ActiveEvents: 재개할 Story Flow 이벤트 목록 (예: State.Player.InWorld)
     - EntityStates: 소유 엔티티의 전체 스냅샷 (TArray<FHktEntityState>)
  3. 로드 완료 시 PendingLoginResults 큐에 적재
       │
       ▼
Phase 3: Relevancy 배치 및 엔티티 주입 (GameModeTick에서)
  1. PendingLoginResults 디큐
  2. LastPosition 기반으로 RelevancyGroup 인덱스 결정
  3. GroupEventSend.Entered에 신규 플레이어 등록
  4. FHktPlayerRecord.EntityStates → GroupBatch.NewEntityStates로 주입
  5. FHktPlayerRecord.ActiveEvents → GroupBatch.NewEvents로 주입
       │
       ▼
Phase 4: 시뮬레이터 실행 및 월드 존재
  1. Simulator.AdvanceFrame(GroupBatch) 호출
  2. NewEntityStates → WorldState.ImportEntityState()로 복원
  3. NewEvents 중 "State.Player.InWorld" → Story VM 기동
     - 신규: 캐릭터 엔티티 SpawnEntity + 초기 아이템 지급
     - 복귀: DB에서 복원된 엔티티가 이미 존재, Story가 상태 유지
  4. Graph.RegisterPlayer(Player, GroupIndex) 실행
  5. 클라이언트에 SimulationEvent + WorldState 전송
```

### 2.5 로그아웃 및 영구 저장 흐름

```
  1. GameMode.Logout() → ServerRule.OnEvent_GameModeLogout()
  2. PendingLogoutRequests 큐에 PlayerUid 적재
  3. GameModeTick에서 디큐:
     a. Simulator.ExportPlayerState(PlayerUid) 호출
        → OwnerUid가 일치하는 모든 엔티티를 FHktEntityState로 추출
        → 해당 플레이어의 ActiveEvents(VM 실행 중인 Story)도 추출
     b. DB.SavePlayerRecordAsync(PlayerUid, PlayerState)
     c. GroupBatch.RemovedOwnerIds에 추가
        → 시뮬레이터가 해당 OwnerUid 엔티티 모두 제거
  4. Graph.UnregisterPlayer(PlayerUid)
```

### 2.6 클라이언트의 자기 Entity 식별

클라이언트는 자신의 `PlayerUid`를 알고 있다 (`UHktWorldPlayerComponent.GetPlayerUid()`).
서버로부터 수신한 `FHktWorldState`에서 `ForEachEntityByOwner(PlayerUid, ...)`를 사용하거나,
엔티티의 `OwnerUid`를 비교하여 자신의 캐릭터와 소유 아이템을 식별한다.

---

## 3. 아이템 소유 정책

### 3.1 설계 원칙

**"아이템은 엔티티이다."**

아이템은 별도의 인벤토리 테이블이 아니라, 캐릭터와 동일한 Entity 시스템 내에 존재한다.
소유 관계는 Property와 OwnerUid로 표현된다.

### 3.2 OwnerUid 자동 전파

`Op_SpawnEntity` (`HktVMInterpreterActions.cpp`)에서:
- `OwnerEntity`는 `Reg::Self`(호출자 엔티티)로 자동 설정
- `OwnerUid`는 `Runtime.PlayerUid`가 0이 아닐 때 자동 설정

따라서 플레이어 Story에서 SpawnEntity로 생성된 아이템은 자동으로 해당 플레이어 계정에 귀속된다.

### 3.3 소유 상태 전이 규칙

```
[무주물 (Ground)]          [소유됨 (InBag/Active)]
  OwnerEntity = 0            OwnerEntity = 캐릭터ID
  OwnerUid = 0               OwnerUid = 플레이어Uid
  ItemState = 0              ItemState = 1 또는 2

      ──── Pickup ────►
      ◄──── Drop ──────
```

| 전이 | Story | 사전 조건 | 변경 속성 |
|------|-------|-----------|-----------|
| **Pickup** | `Story.Event.Item.Pickup` | ItemState==0, 거리<=300cm, 가방<BagCapacity | OwnerEntity=Self, ItemState=1, BagSlot=현재개수 |
| **Grant** | (Story 내부 패턴) | 가방<BagCapacity | SpawnEntity→OwnerEntity=Self, ItemState=1, BagSlot=현재개수 |
| **Activate** | `Story.Event.Item.Activate` | ItemState==1, OwnerEntity==Self | ItemState=2, ActionSlot=Param0, 캐릭터 Stance=아이템 Stance |
| **Deactivate** | `Story.Event.Item.Deactivate` | ItemState==2, OwnerEntity==Self | ItemState=1, ActionSlot=-1 |
| **Drop** | `Story.Event.Item.Drop` | OwnerEntity==Self | ItemState=0, OwnerEntity=0, BagSlot=0, ActionSlot=-1, 위치=Self위치 |

### 3.4 아이템 획득 경로 (Acquisition Paths)

아이템 획득은 **Event(최초 트리거)와 Story(파생 로직)의 구분**(섹션 1.3 참조)에 따라 분류된다.

| 경로 | 분류 | 최초 트리거 (Event) | 파생 로직 (Story) |
|------|------|---------------------|-------------------|
| **바닥 줍기** | Client Intent | `Story.Event.Item.Pickup` | — (Event 자체가 획득 로직) |
| **퀘스트 보상** | Client Intent | 퀘스트 완료 UI 등 | Story 내부에서 Grant (SpawnEntity→InBag) |
| **NPC 상점** | Client Intent | NPC 대화/구매 선택 | Story 내부에서 Grant (SpawnEntity→InBag) |
| **조합** | Client Intent | `Story.Event.Item.Craft` (향후) | 재료소비 후 Grant |
| **NPC 전리품** | Map Event | `Story.Flow.NPC.Lifecycle` | SpawnEntity→Ground → 플레이어가 Pickup |
| **자연 스폰** | Map Event | `Story.Flow.Spawner.Item.TreeDrop` | SpawnEntity→Ground → 플레이어가 Pickup |
| **초기 지급** | Map Event | `Story.State.Player.InWorld` | Story 내부에서 직접 SpawnEntity→InBag |

**Grant는 Event가 아니다.** Grant(아이템 생성+InBag 주입)는 다양한 Event의 Story 내부에서 수행되는 공통 패턴이다. VM에 서브루틴 호출이 없으므로, Grant 로직(용량검증+BagSlot할당+SpawnEntity+속성설정)은 각 Story에 인라인으로 포함된다.

**Pickup과 Grant의 차이:**
- Pickup: 이미 존재하는 Ground 아이템(Target)의 소유권을 가져온다. 거리 검증 필요. 그 자체가 Event.
- Grant: 새 아이템을 SpawnEntity로 생성하여 바로 InBag에 넣는다. 거리 검증 불필요. Event의 Story 내부 로직.

### 3.5 소유 제한 사항

| 제한 | 현재 값 | 근거 |
|------|---------|------|
| 가방 용량 (기본) | 8개 | 엔티티별 `BagCapacity` Cold Property. 플레이어 캐릭터 기본값 8 |
| 가방 용량 (엔티티별 가변) | 엔티티마다 다름 | 이동형 창고 엔티티 등으로 확장 가능 |
| 줍기 최대 거리 | 300cm (3m) | `HktStoryItemPickup.cpp` — `.LoadConst(R1, 300)` |
| 소유권 이전 | Drop 후 Pickup만 가능 | 직접 트레이드 Story 미구현 |
| ActionSlot 범위 | 정수, -1=미등록 | 최대 슬롯 수 미정의 |

---

## 4. 아이템 활성화 정책

### 4.1 아이템 상태 머신

Equip 개념은 존재하지 않는다. 아이템은 **2단계**(Pickup→Activate)로 사용된다.

```
                    ┌──────────┐
        Spawn       │          │
    ───────────────►│  Ground  │
    (자연 스포너)    │ State=0  │
                    └────┬─────┘
                         │
                    Pickup│(Story.Story.Event.Item.Pickup)
                         │ 조건: 거리<=3m, 가방<BagCapacity
                         ▼
                    ┌──────────┐
                    │          │  Deactivate
                    │  InBag   │◄──────────────┐
                    │ State=1  │               │
                    └────┬─────┘               │
                         │                     │
                  Activate│(Story.Event.Item.Activate)│
                         │ Param0 = ActionSlot │
                         │ + Stance 자동 변경   │
                         ▼                     │
                    ┌──────────────┐            │
                    │ Active       │────────────┘
                    │ State=2      │ (Story.Event.Item.Deactivate)
                    │ ActionSlot=N │
                    └──────────────┘

    * Drop은 InBag 또는 Active 어디서든 Ground로 복귀 가능
    * Drop 시: OwnerEntity=0, BagSlot=0, ActionSlot=-1, 위치=소유자위치
    * Activate 시: 아이템의 Stance Property를 읽어 캐릭터 Stance 자동 변경
```

### 4.2 슬롯 구조

**BagSlot (가방 슬롯)**
- 범위: 0~(BagCapacity-1), 기본 BagCapacity=8
- 엔티티별 `BagCapacity` Property로 용량이 다를 수 있음 (이동형 창고 엔티티 등)
- 할당 방식: Pickup 시점에 현재 소유 아이템 수(CountByOwner)를 BagSlot으로 사용
- 용도: 인벤토리 UI에서 아이템 위치 결정

**ActionSlot (액션 슬롯)**
- 범위: -1 (미등록) 또는 0 이상의 정수
- 할당 방식: Event.Item.Activate의 Param0으로 지정
- 용도: 장착된 아이템의 활성 슬롯 (예: 0=주무기, 1=보조장비)
- 현재 사용 예시:
  - `HktStoryCharacterSpawn.cpp`: Sword=ActionSlot 0, Shield=ActionSlot 1
  - `HktStoryPlayerInWorld.cpp`: WoodenSword=ActionSlot -1 (미등록, InBag 상태)

### 4.3 Stance (전투 자세)와 아이템의 관계

Stance는 Hot Property로 캐릭터의 전투 모드를 정의한다:
- `Entity.Stance.Unarmed` — 비무장
- `Entity.Stance.Spear` — 창
- `Entity.Stance.Gun` — 총
- `Entity.Stance.Sword1H` — 한손검

아이템 엔티티는 자신의 `Stance` Property에 해당 무기의 Stance 값을 저장한다.
Activate Story에서 아이템의 Stance를 읽어 캐릭터의 Stance를 자동 변경한다.

### 4.4 아이템 Property 목록

| Property | Tier | 용도 |
|----------|------|------|
| `OwnerEntity` | Hot | 소유 캐릭터 EntityId |
| `AttackPower` | Hot | 공격력 (아이템이 기여하는 전투 스탯) |
| `Defense` | Hot | 방어력 |
| `ItemState` | Cold | 상태 (0=Ground, 1=InBag, 2=Active) |
| `ItemId` | Cold | 아이템 종류 식별자 (100=목검, 101=나무) |
| `BagSlot` | Cold | 가방 내 위치 (0~BagCapacity-1) |
| `ActionSlot` | Cold | 액션 슬롯 번호 (-1=미등록) |
| `BagCapacity` | Cold | 엔티티별 가방 용량 (기본 8, 창고 엔티티 등은 다른 값) |

---

## 5. 현재 구현 상태 매핑

### 5.1 구현 완료 항목

| 기능 | 구현 파일 | 상태 |
|------|-----------|------|
| Entity 할당/해제 | `HktWorldState.h` — `AllocateEntity()`, `RemoveEntity()` | 완료 |
| 3-Tier Property Storage | `HktWorldState.h` — Hot/Warm/Overflow | 완료 |
| OwnerUid 관리 | `HktWorldState.h` — `GetOwnerUid()`, `SetOwnerUid()`, `ForEachEntityByOwner()` | 완료 |
| 태그 기반 분류 | `FGameplayTagContainer` per entity | 완료 |
| 플레이어 접속 흐름 | `HktServerRule.cpp` — `OnEvent_GameModePostLogin()` | 완료 |
| DB 비동기 로드/저장 | `IHktWorldDatabase` 인터페이스 | 완료 |
| 엔티티 직렬화/복원 | `FHktEntityState`, `ExtractEntityState()`, `ImportEntityState()` | 완료 |
| ExportPlayerState | `IHktDeterminismSimulator` — OwnerUid 기준 추출 | 완료 |
| OwnerUid 자동 전파 | `HktVMInterpreterActions.cpp` — `Op_SpawnEntity` | 완료 |
| 아이템 Pickup Flow | `HktStoryItemPickup.cpp` — 거리/용량 검증 포함 | 완료 |
| 아이템 Activate Flow | `HktStoryItemActivate.cpp` — InBag→Active + ActionSlot + Stance | 수정 필요 |
| 아이템 Drop Flow | `HktStoryItemDrop.cpp` — 소유 해제 + 위치 이동 | 완료 |
| 자연 아이템 스포너 | `HktStoryItemSpawnerTreeDrop.cpp` — 30초 주기 나무 스폰 | 완료 |
| 플레이어 월드 진입 | `HktStoryPlayerInWorld.cpp` — 캐릭터 + 목검 생성 | 완료 |
| 캐릭터 스폰 연출 | `HktStoryCharacterSpawn.cpp` — 이펙트/애니메이션/장비 | 완료 |
| 로그아웃 처리 | `HktServerRule.cpp` — Export + Save | 완료 |
| Relevancy 그룹 기반 병렬 시뮬레이션 | `HktServerRule.cpp` — `ParallelFor(NumGroups, ...)` | 완료 |
| WorldView 읽기 뷰 | `HktWorldView.h` — Diff 기반 클라이언트 상태 전달 | 완료 |

### 5.2 아이템 종류 정의

| ItemId | 태그 | 이름 | 초기 스탯 |
|--------|------|------|-----------|
| 100 | `Entity.Item.WoodenSword` | 목검 | AttackPower=5, Stance=Sword1H |
| 101 | `Entity.Item.Wood` | 나무 | (재료) |
| 102 | `Entity.Item.WoodSpear` | 나무창 | AttackPower=7, Stance=Spear |
| - | `Entity.Item.Sword` | 검 | (CharacterSpawn용) |
| - | `Entity.Item.Shield` | 방패 | (CharacterSpawn용) |

---

## 6. Gap 분석 (미구현/불완전 항목)

### Gap 0: Event 검증 레이어 부재 — 우선순위: 높음
- **현상**: Client Intent로 fire되는 Event(최초의 Story)에 대한 사전 검증이 없다. 현재는 Story 내부의 조건 분기(예: Pickup의 거리/용량 체크)로만 검증하고 있다.
- **영향**: 클라이언트가 임의의 Event Tag를 fire하여 부정 행위가 가능하다. Story 내부 검증은 Story가 이미 실행된 후에 발생하므로, 실행 자체를 막지는 못한다.
- **제안**: Event fire 시점에 사전 검증 레이어 추가. Map Event(서버 자체 fire)는 검증 최소화 또는 생략 가능. Client Intent는 Story 실행 전 게이트에서 조건을 확인.

### Gap 1: Deactivate 흐름 부재 — 우선순위: 높음
- **현상**: Active(State=2) → InBag(State=1) 전이를 담당하는 Story가 없다.
- **영향**: 활성 해제를 하려면 Drop 후 Pickup해야 한다 (비직관적).
- **제안**: `Story.Event.Item.Deactivate` Story 추가. Active→InBag 전환, ActionSlot=-1로 초기화, Stance 복원.

### Gap 2: BagSlot 재배치 미구현 — 우선순위: 높음
- **현상**: Drop 시 BagSlot이 0으로 초기화되지만, 나머지 아이템의 BagSlot이 재정렬되지 않는다.
- **영향**: 가방 중간에 빈 슬롯이 생기고, Pickup 시 CountByOwner를 BagSlot으로 사용하므로 슬롯 충돌 가능.
- **제안**: 빈 슬롯 탐색 로직 추가. `FindFirstEmptyBagSlot` VM 명령 또는 Story 로직으로 구현.
- **참고**: 엔티티별 BagCapacity가 다를 수 있으므로 (이동형 창고 등), 슬롯 탐색은 해당 엔티티의 BagCapacity 범위 내에서 수행해야 한다.

### Gap 3: ActionSlot 충돌 미검증 — 우선순위: 중간
- **현상**: `Story.Event.Item.Activate`에서 동일 ActionSlot에 이미 다른 아이템이 할당되어 있는지 확인하지 않는다.
- **영향**: 두 개의 아이템이 같은 ActionSlot을 점유할 수 있다.
- **제안**: 기존 ActionSlot 점유 아이템의 자동 해제 로직 추가.

### Gap 4: 장비 스탯 캐릭터 반영 미구현 — 우선순위: 중간
- **현상**: 아이템의 AttackPower/Defense가 캐릭터의 전투 스탯에 합산되는 로직 없음.
- **영향**: 아이템 활성화가 실질적인 전투력 변화를 일으키지 않음.
- **제안**: Activate/Deactivate Story에서 캐릭터 스탯에 아이템 스탯을 가감산.

### Gap 5: 무기 메쉬 소켓 부착 시스템 부재 — 우선순위: 높음
- **현상**: Presentation 레이어에 무기 소켓 부착 시스템이 없다. 아이템은 독립 Actor로 렌더링되며 캐릭터에 붙지 않는다.
- **영향**: ActionSlot이 변경되어도 시각적으로 무기가 캐릭터에 표시되지 않음.
- **제안**: 캐릭터 SkeletalMesh에 무기 소켓 정의, Activate 시 해당 아이템의 Mesh를 소켓에 Attach, HktActorRenderer에서 ActionSlot 변경 감지.

### Gap 6: 아이템 거래/이전 시스템 부재 — 우선순위: 낮음
- **현상**: 플레이어 간 직접 아이템 이전 수단 없음.
- **영향**: Drop→Pickup으로만 거래 가능 (분실 위험, 보안 취약).
- **제안**: `Story.Event.Item.Trade` Story 설계. 양측 동의 확인 메커니즘 (2-phase commit).

### Gap 7: 신규 vs 복귀 플레이어 분기 — 우선순위: 미정
- **현상**: 복귀 플레이어 재접속 시 DB에서 EntityStates가 Import된 후 `Story.State.Player.InWorld` Story가 다시 기동되면, 초기 아이템(목검)이 중복 지급될 수 있다.
- **현재 상태**: 의도된 동작인지 미정. 추후 결정 필요.

---

## 7. 프로토타입 전투 흐름

### 7.1 기본 흐름

```
1. 서버가 플레이어 Entity 생성 (State.Player.InWorld)
2. 클라이언트가 자기 Entity를 포커싱하여 게임 진행
3. 우클릭으로 이동
4. PrototypeMap에 WoodSpear가 하나 스폰되어 있음
5. 플레이어가 WoodSpear를 Pickup (Client Intent → Story.Event.Item.Pickup)
6. Command로 Activate 실행 (Client Intent → Event.Item.Activate)
   - UI 미구현이므로 Command로 대체
7. Activate 시:
   - ItemState: 1(InBag) → 2(Active)
   - ActionSlot 할당
   - 아이템의 Stance Property를 읽어 캐릭터 Stance 자동 변경
   - 캐릭터 무기 소켓에 해당 아이템 Mesh 부착 (Gap 5)
```

### 7.2 미구현 항목 (프로토타입용)

| 항목 | 상태 | 설명 |
|------|------|------|
| WoodSpear 맵 스폰 Story | 미구현 | PrototypeMap 로드 시 고정 위치에 WoodSpear 1개 스폰하는 Map Event |
| Command → Activate 연결 | 미구현 | 클라이언트에서 키/커맨드로 Event.Item.Activate를 fire하는 입력 경로 |
| 무기 메쉬 소켓 부착 | 미구현 | Presentation 레이어에서 ActionSlot 변경 감지 → 무기 Mesh를 캐릭터 소켓에 Attach |

---

## 8. 향후 확장 제안

### 8.1 아이템 내구도/소비 시스템
- Cold Property로 `Durability` 추가.
- 사용/전투 시 감소, 0이면 파괴(RemoveEntity).

### 8.2 아이템 조합/성장 시스템
- CLAUDE.md에 명시된 "item attribute and combination, random growth" 컨셉.
- **조합**: 두 아이템 엔티티를 소비하고 새 아이템 엔티티를 SpawnEntity.
- **성장**: 아이템 Property에 Level/Experience 추가, Story에서 조건 충족 시 스탯 증가.
