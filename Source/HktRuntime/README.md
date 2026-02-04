HktRuntime 동기화 구조
아키텍처 개요
┌─────────────────────────────────────────────────────────────────┐
│                         SERVER                                   │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    AHktGameMode                          │   │
│  │  ┌─────────────────┐  ┌──────────────────────────────┐  │   │
│  │  │ MasterStash     │  │ GridRelevancy                │  │   │
│  │  │ (전체 엔티티)    │  │ (클라이언트별 관심 셀 Set)   │  │   │
│  │  └─────────────────┘  └──────────────────────────────┘  │   │
│  │                                                          │   │
│  │  CollectedIntents ─────► ProcessFrame() ─────► Batches  │   │
│  └─────────────────────────────────────────────────────────┘   │
│                              │                                   │
│         ┌────────────────────┼────────────────────┐             │
│         ▼                    ▼                    ▼             │
│  ┌─────────────┐      ┌─────────────┐      ┌─────────────┐     │
│  │ PlayerCtrl  │      │ PlayerCtrl  │      │ PlayerCtrl  │     │
│  │ (Relevancy) │      │ (Relevancy) │      │ (Relevancy) │     │
│  └─────────────┘      └─────────────┘      └─────────────┘     │
└────────┬─────────────────────┬─────────────────────┬────────────┘
         │ RPC                 │ RPC                 │ RPC
─────────┼─────────────────────┼─────────────────────┼────────────
         ▼                     ▼                     ▼
┌─────────────┐         ┌─────────────┐       ┌─────────────┐
│ PlayerCtrl  │         │ PlayerCtrl  │       │ PlayerCtrl  │
│┌───────────┐│         │┌───────────┐│       │┌───────────┐│
││VisibleSta ││         ││VisibleSta ││       ││VisibleSta ││
││(부분 복제)││         ││(부분 복제)││       ││(부분 복제)││
│└───────────┘│         │└───────────┘│       │└───────────┘│
│┌───────────┐│         │┌───────────┐│       │┌───────────┐│
││IntentBuild││         ││IntentBuild││       ││IntentBuild││
│└───────────┘│         │└───────────┘│       │└───────────┘│
└─────────────┘         └─────────────┘       └─────────────┘
   CLIENT A                CLIENT B              CLIENT C

데이터 흐름
C2S: Intent 전송
Client                                    Server
──────                                    ──────
IntentBuilder
    │ BeginIntent()
    │ SetSource/Target()
    │ AddPayload()
    │ Build()
    ▼
PlayerController
    │ SendIntent()
    │
    │ Server_ReceiveIntent() ──────────► PlayerController
    │               RPC                       │
                                              ▼
                                         GameMode
                                              │ PushIntent()
                                              │ (Lock 보호)
                                              ▼
                                         CollectedIntents[]
S2C: Batch 전송
Server                                    Client
──────                                    ──────
GameMode::Tick()
    │
    ▼
ProcessFrame()
    │
    ├─ 1. 셀 정보 계산 (이벤트별)
    │      MasterStash->TryGetPosition(SourceEntity)
    │      GridRelevancy->LocationToCell()
    │
    ├─ 2. ParallelFor (클라이언트별)
    │      ├─ 관련 이벤트 필터링 (O(1) 셀 체크)
    │      ├─ Relevancy 진입/이탈 처리
    │      └─ 스냅샷/제거 목록 생성
    │
    └─ 3. 배치 전송
           │
           │ Client_ReceiveBatch() ──────► PlayerController
           │               RPC                  │ (즉시 처리)
                                                ▼
                                           VisibleStash
                                                │ RemoveEntity()
                                                │ ApplySnapshot()
                                                ▼
                                           VMProcessor (TODO)

핵심 타입
cpp// C2S: 클라이언트 의도
struct FHktIntentEvent {
    FHktEntityId SourceEntity;    // Relevancy 계산 기준
    FHktEntityId TargetEntity;
    FGameplayTag EventTag;
    TArray<uint8> Payload;
    bool bIsGlobal;
};

// S2C: 프레임 배치
struct FHktFrameBatch {
    int32 FrameNumber;
    TArray<FHktEntitySnapshot> Snapshots;     // 새로 진입한 엔티티
    TArray<FHktEntityId> RemovedEntities;     // 이탈한 엔티티
    TArray<FHktIntentEvent> Events;           // 이벤트들
};

// 클라이언트별 Relevancy 상태
struct FHktClientRelevancy {
    TSet<FHktEntityId> RelevantEntities;      // 현재 알고 있는 엔티티
    TArray<FHktEntityId> EnteredEntities;     // 이번 프레임 진입
    TArray<FHktEntityId> ExitedEntities;      // 이번 프레임 이탈
};

클래스 역할
클래스위치역할AHktGameMode서버Intent 수집, 병렬 분배, 배치 전송AHktPlayerController양측RPC 처리, Relevancy 상태 보유UMasterStashComponent서버전체 엔티티 SOA 데이터UVisibleStashComponent클라부분 엔티티 SOA 데이터UHktGridRelevancyComponent서버공간 Relevancy (셀 기반)UHktIntentBuilderComponent클라Intent 조립 (빌더 패턴)

병렬 처리 구조
cppProcessFrame() {
    // 1. 메인 스레드: 이벤트별 셀 계산
    for (Event : FrameIntents)
        EventCellCache[i] = LocationToCell(MasterStash->GetPosition(Event.SourceEntity));

    // 2. 병렬: 클라이언트별 독립 처리
    ParallelFor(NumClients, [&](int32 i) {
        PC = AllClients[i];
        Batch = Batches[i];           // 각자 독립
        
        for (Event : FrameIntents)
            if (IsClientInterestedInCell(PC, EventCellCache[j].Cell))  // O(1)
                Batch.Events.Add(Event);
                
        // Relevancy 처리 (각 PC 독립)
        for (EntityId : Relevancy.EnteredEntities)
            Batch.Snapshots.Add(MasterStash->CreateSnapshot(EntityId));
    });

    // 3. 메인 스레드: RPC 전송
    for (PC : AllClients)
        PC->SendBatchToOwningClient(Batch);
}
```

**복잡도**: O(E) + O(C×E) 병렬 + O(C) = 실질적으로 **O(E) + O(C)**

---

### Relevancy 생명주기
```
엔티티 A가 클라이언트 B의 관심 영역에...

진입 시:
┌──────────────────────────────────────────────────┐
│ 1. Event에 A가 Source/Target으로 포함            │
│ 2. B의 관심 셀에 A 위치 포함                      │
│ 3. Relevancy.EnterRelevancy(A)                  │
│ 4. EnteredEntities에 추가                        │
│ 5. Batch.Snapshots에 A 스냅샷 추가              │
│ 6. B는 A의 전체 상태 수신                        │
└──────────────────────────────────────────────────┘

체류 중:
┌──────────────────────────────────────────────────┐
│ - A 관련 이벤트는 B에게 계속 전달                 │
│ - 스냅샷은 전송하지 않음 (이미 알고 있음)         │
│ - B는 이벤트로 A 상태 갱신                       │
└──────────────────────────────────────────────────┘

이탈 시:
┌──────────────────────────────────────────────────┐
│ 1. A가 B의 관심 셀을 벗어남                       │
│ 2. Relevancy.ExitRelevancy(A)                   │
│ 3. ExitedEntities에 추가                         │
│ 4. Batch.RemovedEntities에 A 추가               │
│ 5. B는 A를 VisibleStash에서 제거                │
└──────────────────────────────────────────────────┘

재진입 시:
┌──────────────────────────────────────────────────┐
│ - 진입 시와 동일하게 처리                         │
│ - 스냅샷 다시 전송 (stale 방지)                  │
└──────────────────────────────────────────────────┘
```

---

### 파일 구조
```
HktSimulation/
├── Public/
│   ├── HktIntentTypes.h           # 핵심 타입 정의
│   ├── HktGameMode.h              # 서버 오케스트레이션
│   ├── HktPlayerController.h      # RPC, Relevancy
│   ├── HktRelevancyInterface.h    # Relevancy 추상화
│   ├── HktGridRelevancyComponent.h # 그리드 기반 구현
│   ├── HktIntentBuilderComponent.h # Intent 빌더
│   ├── MasterStashComponent.h     # 서버 SOA
│   └── VisibleStashComponent.h    # 클라 SOA
└── Private/
    └── *.cpp

설계 원칙

컴포넌트는 데이터/RPC만 - 로직은 Controller/GameMode
컴포넌트 간 직접 통신 X - 델리게이트도 X
SourceEntity 위치 기반 Relevancy - 클라 조작 방지
Relevancy 이탈 시 Unknown - stale 데이터 방지
클라이언트 단위 병렬 처리 - O(1) 셀 체크
즉시 처리 - 불필요한 큐 제거