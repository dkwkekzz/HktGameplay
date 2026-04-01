# HktVoxel 모듈 — 구조와 작동 방식

UE5 복셀 렌더링 파이프라인. 결정론적 VM이 소유하는 복셀 데이터를 Greedy Meshing으로 시각화한다.

---

## 전체 데이터 흐름

```
┌─ VM (HktCore) ─────────────────────────────────────────────────────────┐
│                                                                        │
│  VoxelGrid (결정론적 복셀 상태)                                          │
│       │                                                                │
│       ├─ AdvanceFrame() 실행                                            │
│       │                                                                │
│       └─ FHktSimulationDiff.VoxelDeltas[] 발행                          │
│              │                                                         │
└──────────────┼─────────────────────────────────────────────────────────┘
               │
               ▼
┌─ HktVoxelCore (UE5 프레젠테이션) ──────────────────────────────────────┐
│                                                                        │
│  FHktVoxelRenderCache                                                  │
│  (VM 복셀의 읽기 전용 사본)                                               │
│       │                                                                │
│       ├─ ApplyVoxelDelta()  ← Game Thread                              │
│       │                                                                │
│       ▼                                                                │
│  FHktVoxelMeshScheduler                                                │
│  (dirty 청크를 카메라 거리 순으로 정렬)                                    │
│       │                                                                │
│       ├─ UE::Tasks::Launch()  ← Worker Thread                          │
│       ▼                                                                │
│  FHktVoxelMesher::MeshChunk()                                          │
│  (Binary Greedy Meshing + Baked AO)                                    │
│       │                                                                │
│       ├─ bMeshReady = true                                             │
│       ▼                                                                │
│  UHktVoxelChunkComponent::OnMeshReady()                                │
│       │                                                                │
│       ├─ ENQUEUE_RENDER_COMMAND  ← Render Thread                       │
│       ▼                                                                │
│  FHktVoxelChunkProxy::UpdateMeshData_RenderThread()                    │
│  (GPU 버퍼 업로드)                                                       │
│       │                                                                │
│       ▼                                                                │
│  GetDynamicMeshElements() → 화면 렌더링                                  │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 모듈 구조

```
Source/
├── HktVoxelCore/           복셀 렌더링 엔진 코어
│   ├── Public/
│   │   ├── Data/
│   │   │   ├── HktVoxelTypes.h           FHktVoxel, FHktVoxelChunk, FHktVoxelDelta
│   │   │   └── HktVoxelRenderCache.h     VM 복셀 상태의 읽기 전용 사본 관리
│   │   ├── Meshing/
│   │   │   ├── HktVoxelVertex.h          8바이트 압축 버텍스
│   │   │   ├── HktVoxelMesher.h          Binary Greedy Meshing
│   │   │   └── HktVoxelMeshScheduler.h   비동기 메싱 스케줄러
│   │   ├── Rendering/
│   │   │   ├── HktVoxelVertexFactory.h   커스텀 Vertex Factory
│   │   │   ├── HktVoxelChunkProxy.h      SceneProxy (GPU 버퍼 관리)
│   │   │   └── HktVoxelChunkComponent.h  청크 단위 PrimitiveComponent
│   │   ├── LOD/
│   │   │   └── HktVoxelLOD.h             거리 기반 4레벨 LOD
│   │   ├── IHktVoxelCoreModule.h
│   │   └── HktVoxelCoreLog.h
│   ├── Shaders/
│   │   ├── HktVoxelCommon.ush            셰이더 공통 (법선, AO, 쿼드 확장)
│   │   ├── HktVoxelVertexFactory.ush     버텍스 언팩 + 팔레트 룩업
│   │   └── HktVoxelBasePass.usf          GBuffer 출력
│   └── HktVoxelCore.Build.cs
│
├── HktVoxelVFX/            복셀 타격감/이펙트
│   ├── Public/
│   │   ├── HktVoxelVFXDispatcher.h       VM 이벤트 → VFX 변환 허브
│   │   ├── HktVoxelHitFeedback.h         히트스탑/카메라 셰이크 파라미터
│   │   ├── HktVoxelVFXTypes.h            이벤트 타입 정의
│   │   ├── IHktVoxelVFXModule.h
│   │   └── HktVoxelVFXLog.h
│   └── HktVoxelVFX.Build.cs
│
├── HktVoxelSkin/           스킨/팔레트 시스템
│   ├── Public/
│   │   ├── HktVoxelPalette.h             팔레트 텍스처 관리 (256×8 RGBA8)
│   │   ├── HktVoxelSkinAssembler.h       7레이어 모듈러 스킨 조합
│   │   ├── HktVoxelSkinTypes.h           레이어/스킨 ID 정의
│   │   ├── IHktVoxelSkinModule.h
│   │   └── HktVoxelSkinLog.h
│   └── HktVoxelSkin.Build.cs
```

### 의존성 그래프

```
HktCore
  └─ FHktSimulationDiff.VoxelDeltas (데이터 흐름만, 모듈 의존 없음)

HktVoxelCore
  ├─ Core, CoreUObject, Engine
  ├─ RenderCore, RHI, Renderer    ← GPU 레벨 접근
  └─ Projects                      ← 셰이더 경로 매핑

HktVoxelVFX
  ├─ HktVoxelCore
  └─ Niagara                       ← 파편 파티클

HktVoxelSkin
  └─ HktVoxelCore
```

---

## 핵심 타입

### FHktVoxel — 단일 복셀 (4바이트)

```cpp
struct FHktVoxel
{
    uint16 TypeID;         // 복셀 종류 (0 = 빈 공간)
    uint8  PaletteIndex;   // 8색 팔레트 내 인덱스 (0~7)
    uint8  Flags;          // FLAG_TRANSLUCENT | FLAG_EMISSIVE | FLAG_DESTRUCTIBLE | FLAG_ANIMATED
};
```

### FHktVoxelChunk — 청크 (32×32×32 = 32,768 복셀, ~128KB)

```cpp
struct FHktVoxelChunk
{
    static constexpr int32 SIZE = 32;
    FHktVoxel Data[SIZE][SIZE][SIZE];

    FIntVector ChunkCoord;      // VM 기준 청크 좌표
    bool bMeshDirty;            // 재메싱 필요
    bool bMeshReady;            // GPU 업로드 대기

    // Greedy Meshing 결과
    TArray<FHktVoxelVertex> OpaqueVertices;
    TArray<uint32> OpaqueIndices;
    TArray<FHktVoxelVertex> TranslucentVertices;
    TArray<uint32> TranslucentIndices;
};
```

### FHktVoxelVertex — 압축 버텍스 (8바이트)

```
PackedPositionAndSize (32bit):
  ┌──────┬──────┬──────┬───────┬────────┬───────────────┬────┐
  │ x:6  │ y:6  │ z:6  │ w:6   │ h:6    │ face_low:2    │ _  │
  └──────┴──────┴──────┴───────┴────────┴───────────────┴────┘

PackedMaterialAndAO (32bit):
  ┌────────────┬────────┬─────┬───────┬──────────┬────────┐
  │ type:16    │ pal:3  │ao:2 │ flg:3 │face_hi:1 │ _:7    │
  └────────────┴────────┴─────┴───────┴──────────┴────────┘
```

셰이더에서 `SV_VertexID % 4`로 쿼드의 4개 코너를 확장한다. Width/Height는 Greedy Meshing으로 병합된 면의 크기.

### FHktVoxelDelta — VM → 렌더 캐시 변경 이벤트

```cpp
struct FHktVoxelDelta
{
    FIntVector ChunkCoord;      // 대상 청크
    uint16 LocalIndex;          // 청크 내 복셀 인덱스 (0~32767)
    uint16 NewTypeID;
    uint8  NewPaletteIndex;
    uint8  NewFlags;
};
```

`FHktSimulationDiff.VoxelDeltas` 배열로 프레임별 전달된다.

---

## 작동 방식

### 1. VM → 렌더 캐시 동기화

VM이 매 틱 `AdvanceFrame()`을 실행하면 `FHktSimulationDiff`가 생성된다. 이 Diff에 포함된 `VoxelDeltas`를 Game Thread에서 소비:

```cpp
// VMBridge 또는 PresentationSubsystem에서
for (const auto& Delta : Diff.VoxelDeltas)
{
    FHktVoxel NewVoxel;
    NewVoxel.TypeID = Delta.NewTypeID;
    NewVoxel.PaletteIndex = Delta.NewPaletteIndex;
    NewVoxel.Flags = Delta.NewFlags;
    RenderCache->ApplyVoxelDelta(Delta.ChunkCoord, Delta.LocalIndex, NewVoxel);
}
```

`ApplyVoxelDelta()`는 해당 청크의 `bMeshDirty = true`로 마킹한다.

### 2. 메싱 스케줄링

`FHktVoxelMeshScheduler::Tick()`이 매 프레임 호출된다:

1. `RenderCache->GetDirtyChunks()`로 dirty 목록 수집
2. 카메라 거리 기준 정렬 (가까운 청크 우선)
3. 프레임당 최대 `MaxMeshPerFrame`(기본 4)개를 `UE::Tasks`로 비동기 실행

```
Game Thread                    Worker Thread(s)
    │                               │
    ├─ Tick() → dirty 수집           │
    ├─ 정렬 (카메라 거리)             │
    ├─ Tasks::Launch() ──────────▶ MeshChunk()
    │                              ├─ BuildFaceMask() × 6면 × 32슬라이스
    │                              ├─ MergeQuads() (Greedy)
    │                              ├─ CalcVertexAO() (인접 복셀 체크)
    │                              └─ bMeshReady = true
    │                               │
    ├─ OnMeshReady() ◀──────────────┘
    ├─ ENQUEUE_RENDER_COMMAND ───▶ Render Thread
    │                              └─ UpdateMeshData_RenderThread()
    │                                  ├─ CreateVertexBuffer()
    │                                  ├─ CreateIndexBuffer()
    │                                  └─ VertexFactory.SetData()
```

### 3. Binary Greedy Meshing 알고리즘

6면 × 32슬라이스를 순회하며:

**BuildFaceMask()** — 슬라이스의 각 행을 uint32 비트마스크로 표현. 비트가 1이면 해당 복셀의 면이 노출됨 (인접 복셀이 비어있거나 투명).

```
예: 슬라이스 X=5, 면=+X 방향
  행 0: 0b00001111110000  → 4~9번 복셀의 +X면 노출
  행 1: 0b00001111110000  → 동일 패턴
  행 2: 0b00000011110000  → 다른 패턴
```

**MergeQuads()** — 행 내 연속 비트를 찾고(Width), 같은 TypeID+PaletteIndex인 아래 행을 확장(Height):

```
Step 1: 행 0에서 연속 비트 발견 → Width=6
Step 2: 행 1 확인 → 같은 타입, 같은 패턴 → Height=2
Step 3: 행 2 확인 → 패턴 다름 → 확장 중단
결과: 6×2 크기의 하나의 쿼드로 병합 (24개 면 → 1개 쿼드)
```

**CalcVertexAO()** — 쿼드 코너별 인접 3복셀(side1, side2, corner) 체크:

```
AO = 3 - (side1 + side2 + corner)
  3: 차폐 없음 (밝음)
  0: 양쪽+코너 모두 차폐 (어두움)

삼각형 분할 시 AO 대각선 뒤집기로 아티팩트 방지:
  AO[0]+AO[3] > AO[1]+AO[2] → 표준 분할
  그 외 → 뒤집힌 분할
```

### 4. 렌더링 파이프라인

```
렌더 패스 순서:
  Pass 1: 불투명 복셀 (Greedy Meshed) — 전체의 ~85%
  Pass 2: 반투명 복셀 (별도 Greedy Mesh, back-to-front)
  Pass 3: 파티클/파편 (Niagara)
  Pass 4: 포스트프로세싱
```

**FHktVoxelChunkProxy**가 `GetDynamicMeshElements()`에서 FMeshBatch를 생성하여 렌더러에 제출. 머티리얼은 팔레트 기반 단일 머티리얼.

### 5. 셰이더 언팩

Vertex Shader에서 8바이트 → 위치/법선/색상/발광 복원:

```hlsl
// HktVoxelVertexFactory.ush
void HktUnpackVoxelVertex(uint PackedPosSize, uint PackedMaterial, uint VertexID, ...)
{
    // 1. 위치 언팩 (6bit × 3)
    // 2. Width/Height 언팩 → HktExpandQuad()로 코너 확장
    // 3. 면 방향(3bit) → 법선
    // 4. TypeID+PaletteIndex → PaletteTexture.Load() 색상 룩업
    // 5. AO(2bit) → lerp(0.4, 1.0, ao/3) 밝기 조절
    // 6. Flags → 발광 여부
}
```

---

## 타격감 시스템 (HktVoxelVFX)

### 이벤트 흐름

```
VM: 타격 판정 완료
  │
  ├─ FHktSimulationDiff.VFXEvents (기존 경로)
  │
  └─ VMBridge에서 타격 이벤트 파싱
      │
      ▼
  UHktVoxelVFXDispatcher::OnHit()
      │
      ├─ SpawnVoxelFragments()
      │    └─ Niagara Mesh Renderer (1×1×1 큐브)
      │       Color ← 팔레트 룩업
      │       Velocity ← HitDirection 반대 + 랜덤
      │       (파편 물리는 Niagara 자체, VM과 무관)
      │
      ├─ ApplyHitStop()
      │    └─ 보간 알파 일시 정지
      │       Normal: 0.03s, Critical: 0.07s, Kill: 0.15s
      │       ⚠️ VM 틱은 절대 멈추지 않음!
      │
      ├─ ShakeCamera()
      │    └─ UCameraShakeBase 재생
      │
      └─ FlashTarget()
           └─ 머티리얼 파라미터 플래시
```

### 히트스탑 핵심 원리

VM은 30Hz 고정 틱으로 동작하고 렌더링은 60~144Hz이다. 두 틱 사이를 `TickAlpha`(0~1)로 보간한다.

히트스탑은 이 `TickAlpha`의 진행을 일시 멈추는 것:

```
정상 상태:
  VM Tick 0 ─────── VM Tick 1 ─────── VM Tick 2
  Alpha: 0 → 0.5 → 1.0  0 → 0.5 → 1.0

히트스탑 (0.07초):
  VM Tick 0 ─────── VM Tick 1 ─────── VM Tick 2
  Alpha: 0 → 0.5 → [정지 0.07s] → 1.0  0 → ...

  → 화면이 0.07초간 "멈춘 것처럼" 보임
  → VM 내부에서는 Tick 1, Tick 2가 정상 진행
  → 결정론성에 영향 없음
```

---

## 스킨/팔레트 시스템 (HktVoxelSkin)

### 팔레트 텍스처

```
PaletteTexture (256 × 8, RGBA8)
┌─────────────────────────┐
│ Row 0:  기본 팔레트       │  [0] [1] [2] [3] [4] [5] [6] [7]
│ Row 1:  화염 팔레트       │  🟥 🟧 🟨 ⬜ ⬛ ...
│ Row 2:  얼음 팔레트       │  🟦 ⬜ 🟦 ⬜ ⬛ ...
│ ...                      │
│ Row 255: ...             │
└─────────────────────────┘

스킨 교체 = PaletteRow 변경만
  → GPU에서 PaletteTexture.Load(int3(PaletteIndex, Row, 0))
  → 재메싱 불필요, CPU 비용 제로
```

### 7레이어 모듈러 스킨

```
레이어 우선순위 (높은 번호가 덮어쓴다):
  [0] Body     기본 몸체
  [1] Head     머리
  [2] Armor    갑옷/상의
  [3] Boots    부츠/하의
  [4] Gloves   장갑
  [5] Cape     망토/날개
  [6] Weapon   무기

FHktVoxelSkinAssembler::Assemble(OutChunk)
  → 레이어 0부터 순회
  → 각 레이어의 복셀 데이터를 Offset 적용하여 합성
  → 높은 레이어가 겹치는 복셀을 덮어씀
  → 결과 청크를 메싱
```

장착/탈착 → Assemble + 재메싱 (드묾)
색상 변경 → PaletteRow만 변경 (즉시, 비용 없음)

---

## LOD 시스템

```
LOD 0:  32×32×32  (풀 디테일)       거리 < 32m
LOD 1:  16×16×16  (2× 다운샘플)     거리 < 64m
LOD 2:   8× 8× 8  (4× 다운샘플)     거리 < 128m
LOD 3:   4× 4× 4  (8× 다운샘플)     거리 ≥ 128m
```

`FHktVoxelLODPolicy::GetLODLevel(DistanceSquared)` → 메싱 스케줄러가 LOD에 따라 해상도 조절.

---

## 스레드 모델

| 작업 | 스레드 | 비고 |
|------|--------|------|
| VoxelDelta 적용 | Game Thread | `ApplyVoxelDelta()` |
| Dirty 청크 수집/정렬 | Game Thread | `MeshScheduler::Tick()` |
| Greedy Meshing | Worker Thread | `UE::Tasks::Launch()` |
| GPU 버퍼 업로드 | Render Thread | `ENQUEUE_RENDER_COMMAND` |
| 메시 렌더링 | Render Thread | `GetDynamicMeshElements()` |
| VFX 스폰 | Game Thread | `UNiagaraFunctionLibrary` |
| 팔레트 변경 | Game Thread | 즉시 반영 (셰이더 파라미터) |

---

## 소유권 원칙

```
     VM (HktCore)                UE5 (HktVoxelCore)
  ┌──────────────┐           ┌──────────────────┐
  │ VoxelGrid    │──Delta──▶│ RenderCache      │
  │ (유일한 소유자)│           │ (읽기 전용 사본)   │
  │              │           │                  │
  │ 게임 로직     │           │ 시각화만 담당      │
  │ 충돌 판정     │           │ 이펙트 물리 자유   │
  │ 상태 변경     │           │ 상태 변경 금지     │
  └──────────────┘           └──────────────────┘
```

- 복셀 데이터의 유일한 소유자 = VM
- UE5는 렌더링 전용 사본(RenderCache)만 보유
- 1~2틱 지연 허용 (파편 이펙트가 먼저 보이므로 체감 안 됨)
- UE5에서 게임 로직 금지 — 히트스탑 같은 시각 연출과 게임 판정을 혼동하지 않는다

---

## 단계별 렌더링 전략

| Phase | 방법 | 용도 |
|-------|------|------|
| Phase 1 (프로토타입) | ProceduralMeshComponent | VM 연동 검증 |
| Phase 2 (알파) | RealtimeMeshComponent | 성능 개선, LOD |
| Phase 3 (프로덕션) | Custom SceneProxy + VertexFactory | MultiDraw, GigaBuffer |

현재 코드는 Phase 3 구조(SceneProxy + VertexFactory)를 포함하되, Phase 1에서는 PMC로 빠른 검증이 가능하도록 설계되어 있다.
