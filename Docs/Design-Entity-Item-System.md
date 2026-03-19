# Entity 생명주기 및 아이템 소유/활성화 정책 기획서

## 문서 개요

HktGameplay 모듈의 Entity 생명주기(Lifecycle)와 아이템 소유/활성화 정책을 정의하는 게임 기획서이다.
현재 코드베이스의 구현 상태를 기준으로 전체 설계를 정리하고, 미구현 영역과 개선 제안을 포함한다.

---

## 1. Entity 생명주기

### 1.1 핵심 개념

모든 게임 오브젝트(캐릭터, 아이템, NPC, 이펙트)는 **Entity**이다.
Entity는 순수 데이터이며, `FHktEntityId`(int32)로 식별된다.
Entity의 종류는 `FGameplayTagContainer`의 태그로 구분한다 (예: `Entity.Character.Player`, `Entity.Item.Sword`).

### 1.2 식별 체계

| 식별자 | 타입 | 범위 | 용도 |
|--------|------|------|------|
| `FHktEntityId` | int32 | 그룹 내 유일 | 런타임 엔티티 참조 (NextEntityId++ 순차 할당) |
| `PlayerUid` | int64 | 전역 유일 | 계정 식별, DB 키, 엔티티 소유권 |
| `OwnerEntity` | int32 (Hot Property) | 그룹 내 | 엔티티-엔티티 소유 관계 (아이템→캐릭터) |
| `EntitySpawnTag` | int32 (Hot Property) | GameplayTag 넷인덱스 | Presentation 시각 유형 |

### 1.3 소유 관계의 이중 구조

**계정 소유 (Account Ownership)** — `OwnerUid` (int64)
- Entity가 어떤 플레이어 계정에 속하는지 나타낸다.
- DB 저장/로드, 로그아웃 시 엔티티 추출의 기준.
- `Op_SpawnEntity`에서 `Runtime.PlayerUid != 0`일 때 자동 설정된다.

**엔티티 소유 (Entity Ownership)** — `OwnerEntity` (int32, Hot Property)
- 아이템이 어떤 캐릭터 엔티티에 소속되는지 나타낸다.
- Pickup/Equip/Drop 등 Story 검증의 기준.
- `Op_SpawnEntity`에서 `Reg::Self`로 자동 설정된다.

### 1.4 접속에서 월드 존재까지의 전체 흐름

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

### 1.5 로그아웃 및 영구 저장 흐름

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

### 1.6 클라이언트의 자기 Entity 식별

클라이언트는 자신의 `PlayerUid`를 알고 있다 (`UHktWorldPlayerComponent.GetPlayerUid()`).
서버로부터 수신한 `FHktWorldState`에서 `ForEachEntityByOwner(PlayerUid, ...)`를 사용하거나,
엔티티의 `OwnerUid`를 비교하여 자신의 캐릭터와 소유 아이템을 식별한다.

---

## 2. 아이템 소유 정책

### 2.1 설계 원칙

**"아이템은 엔티티이다."**

아이템은 별도의 인벤토리 테이블이 아니라, 캐릭터와 동일한 Entity 시스템 내에 존재한다.
소유 관계는 Property와 OwnerUid로 표현된다.

### 2.2 OwnerUid 자동 전파

`Op_SpawnEntity` (`HktVMInterpreterActions.cpp`)에서:
- `OwnerEntity`는 `Reg::Self`(호출자 엔티티)로 자동 설정
- `OwnerUid`는 `Runtime.PlayerUid`가 0이 아닐 때 자동 설정

따라서 플레이어 Story에서 SpawnEntity로 생성된 아이템은 자동으로 해당 플레이어 계정에 귀속된다.

### 2.3 소유 상태 전이 규칙

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
| **Pickup** | `Event.Item.Pickup` | ItemState==0, 거리<=300cm, 가방<20 | OwnerEntity=Self, ItemState=1, BagSlot=현재개수 |
| **Equip** | `Event.Item.Equip` | ItemState==1, OwnerEntity==Self | ItemState=2 |
| **Activate** | `Event.Item.Activate` | ItemState==2, OwnerEntity==Self | ActionSlot=Param0 |
| **Drop** | `Event.Item.Drop` | OwnerEntity==Self | ItemState=0, OwnerEntity=0, BagSlot=0, ActionSlot=-1, 위치=Self위치 |

### 2.4 소유 제한 사항

| 제한 | 현재 값 | 근거 |
|------|---------|------|
| 가방 용량 (최대 소유 아이템) | 20개 | `HktStoryItemPickup.cpp` — `.LoadConst(R1, 20)` |
| 줍기 최대 거리 | 300cm (3m) | `HktStoryItemPickup.cpp` — `.LoadConst(R1, 300)` |
| 소유권 이전 | Drop 후 Pickup만 가능 | 직접 트레이드 Story 미구현 |
| ActionSlot 범위 | 정수, -1=미등록 | 최대 슬롯 수 미정의 |

---

## 3. 아이템 활성화 정책

### 3.1 아이템 상태 머신

```
                    ┌──────────┐
        Spawn       │          │
    ───────────────►│  Ground  │
    (자연 스포너)    │ State=0  │
                    └────┬─────┘
                         │
                    Pickup│(Event.Item.Pickup)
                         │ 조건: 거리<=3m, 가방<20
                         ▼
                    ┌──────────┐
                    │          │
                    │  InBag   │◄──── (미구현: Unequip)
                    │ State=1  │
                    └────┬─────┘
                         │
                    Equip│(Event.Item.Equip)
                         │ 조건: OwnerEntity==Self
                         ▼
                    ┌──────────┐
                    │          │
                    │  Active  │
                    │ State=2  │
                    └────┬─────┘
                         │
                  Activate│(Event.Item.Activate)
                         │ Param0 = ActionSlot 번호
                         ▼
                    ┌──────────────┐
                    │ Active       │
                    │ + ActionSlot │
                    │   할당됨     │
                    └──────────────┘

    * Drop은 InBag 또는 Active 어디서든 Ground로 복귀 가능
    * Drop 시: OwnerEntity=0, BagSlot=0, ActionSlot=-1, 위치=소유자위치
```

### 3.2 슬롯 구조

**BagSlot (가방 슬롯)**
- 범위: 0~19 (용량 20)
- 할당 방식: Pickup 시점에 현재 소유 아이템 수(CountByOwner)를 BagSlot으로 사용
- 용도: 인벤토리 UI에서 아이템 위치 결정

**ActionSlot (액션 슬롯)**
- 범위: -1 (미등록) 또는 0 이상의 정수
- 할당 방식: Event.Item.Activate의 Param0으로 지정
- 용도: 장착된 아이템의 활성 슬롯 (예: 0=주무기, 1=보조장비)
- 현재 사용 예시:
  - `HktStoryCharacterSpawn.cpp`: Sword=ActionSlot 0, Shield=ActionSlot 1
  - `HktStoryPlayerInWorld.cpp`: WoodenSword=ActionSlot -1 (미등록, InBag 상태)

### 3.3 Stance (전투 자세)와 아이템의 관계

Stance는 Hot Property로 캐릭터의 전투 모드를 정의한다:
- `Stance.Unarmed` — 비무장
- `Stance.Spear` — 창
- `Stance.Gun` — 총
- `Stance.Sword1H` — 한손검

현재 구현에서 Stance는 Story에서 직접 설정하며, 장착 아이템과의 자동 연동은 미구현이다.

### 3.4 아이템 Property 목록

| Property | Tier | 용도 |
|----------|------|------|
| `OwnerEntity` | Hot | 소유 캐릭터 EntityId |
| `AttackPower` | Hot | 공격력 (아이템이 기여하는 전투 스탯) |
| `Defense` | Hot | 방어력 |
| `ItemState` | Cold | 상태 (0=Ground, 1=InBag, 2=Active) |
| `ItemId` | Cold | 아이템 종류 식별자 (100=목검, 101=나무) |
| `BagSlot` | Cold | 가방 내 위치 (0~19) |
| `ActionSlot` | Cold | 액션 슬롯 번호 (-1=미등록) |

---

## 4. 현재 구현 상태 매핑

### 4.1 구현 완료 항목

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
| 아이템 Equip Flow | `HktStoryItemEquip.cpp` — InBag→Active | 완료 |
| 아이템 Activate Flow | `HktStoryItemActivate.cpp` — ActionSlot 할당 | 완료 |
| 아이템 Drop Flow | `HktStoryItemDrop.cpp` — 소유 해제 + 위치 이동 | 완료 |
| 자연 아이템 스포너 | `HktStoryItemSpawnerTreeDrop.cpp` — 30초 주기 나무 스폰 | 완료 |
| 플레이어 월드 진입 | `HktStoryPlayerInWorld.cpp` — 캐릭터 + 목검 생성 | 완료 |
| 캐릭터 스폰 연출 | `HktStoryCharacterSpawn.cpp` — 이펙트/애니메이션/장비 | 완료 |
| 로그아웃 처리 | `HktServerRule.cpp` — Export + Save | 완료 |
| Relevancy 그룹 기반 병렬 시뮬레이션 | `HktServerRule.cpp` — `ParallelFor(NumGroups, ...)` | 완료 |
| WorldView 읽기 뷰 | `HktWorldView.h` — Diff 기반 클라이언트 상태 전달 | 완료 |

### 4.2 아이템 종류 정의

| ItemId | 태그 | 이름 | 초기 스탯 |
|--------|------|------|-----------|
| 100 | `Entity.Item.WoodenSword` | 목검 | AttackPower=5 |
| 101 | `Entity.Item.Wood` | 나무 | (재료) |
| - | `Entity.Item.Sword` | 검 | (CharacterSpawn용) |
| - | `Entity.Item.Shield` | 방패 | (CharacterSpawn용) |

---

## 5. Gap 분석 (미구현/불완전 항목)

### Gap 1: Unequip 흐름 부재 — 우선순위: 높음
- **현상**: Active(State=2) → InBag(State=1) 전이를 담당하는 Story가 없다.
- **영향**: 장착 해제를 하려면 Drop 후 Pickup해야 한다 (비직관적).
- **제안**: `Event.Item.Unequip` Story 추가. Active→InBag 전환, ActionSlot=-1로 초기화.

### Gap 2: BagSlot 재배치 미구현 — 우선순위: 높음
- **현상**: Drop 시 BagSlot이 0으로 초기화되지만, 나머지 아이템의 BagSlot이 재정렬되지 않는다.
- **영향**: 가방 중간에 빈 슬롯이 생기고, Pickup 시 CountByOwner를 BagSlot으로 사용하므로 슬롯 충돌 가능.
- **제안**: 빈 슬롯 탐색 로직 추가. `FindFirstEmptyBagSlot` VM 명령 또는 Story 로직으로 구현.

### Gap 3: ActionSlot 충돌 미검증 — 우선순위: 중간
- **현상**: `Event.Item.Activate`에서 동일 ActionSlot에 이미 다른 아이템이 할당되어 있는지 확인하지 않는다.
- **영향**: 두 개의 아이템이 같은 ActionSlot을 점유할 수 있다.
- **제안**: 기존 ActionSlot 점유 아이템의 자동 해제 로직 추가.

### Gap 4: 장비 스탯 캐릭터 반영 미구현 — 우선순위: 중간
- **현상**: 아이템의 AttackPower/Defense가 캐릭터의 전투 스탯에 합산되는 로직 없음.
- **영향**: 아이템 장착이 실질적인 전투력 변화를 일으키지 않음.
- **제안**: Equip/Unequip Story에서 캐릭터 스탯에 아이템 스탯을 가감산 (이벤트 기반, 매 프레임 계산 방지).

### Gap 5: Stance 자동 연동 부재 — 우선순위: 낮음
- **현상**: Stance는 Story에서 수동 설정. 아이템 장착/해제 시 Stance 자동 변경 없음.
- **영향**: 검을 장착했는데 Spear 자세를 유지하는 등의 불일치 가능.
- **제안**: 아이템 태그(`Tag.Weapon.*`)와 Stance 매핑 테이블 또는 Equip Story 자동화.

### Gap 6: 아이템 거래/이전 시스템 부재 — 우선순위: 낮음
- **현상**: 플레이어 간 직접 아이템 이전 수단 없음.
- **영향**: Drop→Pickup으로만 거래 가능 (분실 위험, 보안 취약).
- **제안**: `Event.Item.Trade` Story 설계. 양측 동의 확인 메커니즘 (2-phase commit).

### Gap 7: 신규 vs 복귀 플레이어 분기 — 우선순위: 미정
- **현상**: 복귀 플레이어 재접속 시 DB에서 EntityStates가 Import된 후 `State.Player.InWorld` Story가 다시 기동되면, 초기 아이템(목검)이 중복 지급될 수 있다.
- **현재 상태**: 의도된 동작인지 미정. 추후 결정 필요.

---

## 6. 향후 확장 제안

### 6.1 아이템 내구도/소비 시스템
- Cold Property로 `Durability` 추가.
- 사용/전투 시 감소, 0이면 파괴(RemoveEntity).

### 6.2 아이템 조합/성장 시스템
- CLAUDE.md에 명시된 "item attribute and combination, random growth" 컨셉.
- **조합**: 두 아이템 엔티티를 소비하고 새 아이템 엔티티를 SpawnEntity.
- **성장**: 아이템 Property에 Level/Experience 추가, Story에서 조건 충족 시 스탯 증가.
