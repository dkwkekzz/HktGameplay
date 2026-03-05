#include "HktProxySimulatorComponent.h"
#include "HktRuntimeCommon.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#include "HktCoreSimulator.h"
#endif

UHktProxySimulatorComponent::UHktProxySimulatorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UHktProxySimulatorComponent::BeginPlay()
{
    Super::BeginPlay();
    SchemaRegistry.Initialize();
    Simulator = CreateDeterminismSimulator(TEXT("Client"));
    HKT_INSIGHTS_REGISTER_PROVIDER(this);
}

void UHktProxySimulatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    HKT_INSIGHTS_UNREGISTER_PROVIDER(this);
    Simulator.Reset();
    Super::EndPlay(EndPlayReason);
}

// ============================================================================
// 매 틱: 고정 타임스텝 로컬 시뮬레이션 → 서버 Batch 조정
// ============================================================================

void UHktProxySimulatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialized) return;

    // 서버 Batch 없을 때만 고정 타임스텝 로컬 예측 실행
    FrameAccumulator += DeltaTime;
    while (FrameAccumulator >= FixedDeltaTime)
    {
        FrameAccumulator -= FixedDeltaTime;

        // 서버 Batch가 있으면 로컬 예측 건너뛰고 바로 조정 처리
        // (어차피 Diff 역적용으로 롤백되므로 로컬 예측은 낭비)
        if (PendingServerBatches.Num() > 0)
        {
            ProcessPendingServerBatches();
        }
        else
        {
            AdvanceLocalFrame(FixedDeltaTime);
        }
    }
}

void UHktProxySimulatorComponent::AdvanceLocalFrame(float DeltaSeconds)
{
    LocalFrame++;

    FHktSimulationEvent LocalBatch = BuildLocalBatch(LocalFrame, DeltaSeconds);
    FHktSimulationDiff Diff = Simulator->AdvanceFrame(LocalBatch);

    // Diff 히스토리에 기록 (역적용 롤백용) — 복사 후 원본에서 PendingDiff로 이동
    DiffHistory.Add(Diff);

    // PendingDiff에 누적 (PlayerController Tick에서 소비 → WorldViewUpdated 전달)
    PendingDiff.FrameNumber = Diff.FrameNumber;
    PendingDiff.SpawnedEntities.Append(MoveTemp(Diff.SpawnedEntities));
    PendingDiff.RemovedEntities.Append(MoveTemp(Diff.RemovedEntities));
    PendingDiff.RemovedEntityStates.Append(MoveTemp(Diff.RemovedEntityStates));
    PendingDiff.PropertyDeltas.Append(MoveTemp(Diff.PropertyDeltas));
    PendingDiff.TagDeltas.Append(MoveTemp(Diff.TagDeltas));
    PendingDiff.OwnerDeltas.Append(MoveTemp(Diff.OwnerDeltas));
    bHasPendingDiff = true;

    // 메모리 보호: 오래된 히스토리 제거
    if (DiffHistory.Num() > MaxHistoryFrames)
    {
        DiffHistory.Empty();
    }
}

FHktSimulationEvent UHktProxySimulatorComponent::BuildLocalBatch(
    int64 Frame, float DeltaSeconds) const
{
    FHktSimulationEvent Batch;
    Batch.FrameNumber = Frame;
    Batch.DeltaSeconds = DeltaSeconds;
    Batch.RandomSeed = HktRuntimeCommon::HashCombineHelper(Frame, 0);
    return Batch;
}

// ============================================================================
// 서버 Batch 큐 적재 (수신 즉시 처리하지 않음)
// ============================================================================

void UHktProxySimulatorComponent::EnqueueServerBatch(const FHktSimulationEvent& InBatch)
{
    PendingServerBatches.Add(InBatch);
}

bool UHktProxySimulatorComponent::ConsumePendingDiff(FHktSimulationDiff& OutDiff)
{
    if (!bHasPendingDiff) return false;
    OutDiff = MoveTemp(PendingDiff);
    bHasPendingDiff = false;
    return true;
}

// ============================================================================
// 서버 Batch 조정 — Diff 역적용으로 롤백(클라 빠름) / 빨리감기(클라 느림)
// ============================================================================

void UHktProxySimulatorComponent::ProcessPendingServerBatches()
{
    // 프레임 번호 기준 오름차순 정렬
    PendingServerBatches.Sort([](const FHktSimulationEvent& A, const FHktSimulationEvent& B)
    {
        return A.FrameNumber < B.FrameNumber;
    });

    FHktSimulationDiff LastDiff;
    bool bProducedDiff = false;

    for (const FHktSimulationEvent& ServerBatch : PendingServerBatches)
    {
        const int64 ServerFrame = ServerBatch.FrameNumber;

        // --- 1. Diff 역적용으로 ServerFrame 직전까지 롤백 ---
        //   DiffHistory는 시간순 정렬 (오래된 것이 앞), 역순으로 Undo
        while (DiffHistory.Num() > 0)
        {
            const FHktSimulationDiff& TopDiff = DiffHistory.Last();
            if (TopDiff.FrameNumber < ServerFrame) break;  // 서버 프레임보다 이전이면 중단
            Simulator->UndoDiff(TopDiff);
            DiffHistory.Pop();
        }

        // --- 2. 클라가 느린 경우: 빈 Batch로 빨리감기 ---
        int64 CurrentFrame = Simulator->GetWorldState().FrameNumber;
        for (int64 F = CurrentFrame + 1; F < ServerFrame; ++F)
        {
            FHktSimulationEvent GapBatch = BuildLocalBatch(F, FixedDeltaTime);
            Simulator->AdvanceFrame(GapBatch);
            // 갭 프레임 Diff는 저장하지 않음 (서버 확정 후 바로 덮어쓰기)
        }

        // --- 3. 서버 권위 Batch로 해당 프레임 실행 → Diff 획득 ---
        LastDiff = Simulator->AdvanceFrame(ServerBatch);
        bProducedDiff = true;

        // --- 4. 기록 초기화 & LocalFrame 보정 ---
        DiffHistory.Empty();
        LocalFrame = FMath::Max(LocalFrame, ServerFrame);
    }

    PendingServerBatches.Reset();

    if (bProducedDiff)
    {
        PendingDiff = MoveTemp(LastDiff);
        bHasPendingDiff = true;
    }
}

// ============================================================================
// InitialState 수신 (그룹 진입 시)
// ============================================================================

void UHktProxySimulatorComponent::RestoreState(const FHktWorldState& InState)
{
    Simulator->RestoreWorldState(InState);

    LocalFrame = InState.FrameNumber;
    DiffHistory.Empty();
    PendingServerBatches.Empty();
    bHasPendingDiff = false;
    FrameAccumulator = 0.0f;

    bInitialized = true;
}

const FHktWorldState& UHktProxySimulatorComponent::GetWorldState() const
{
    return Simulator->GetWorldState();
}

bool UHktProxySimulatorComponent::IsInitialized() const
{
    return bInitialized;
}

// ============================================================================
// Insights
// ============================================================================

#if WITH_HKT_INSIGHTS
void UHktProxySimulatorComponent::CollectInsightData(FHktInsightSnapshot& OutSnapshot) const
{
    OutSnapshot.ProviderName = TEXT("ClientSimulator");

    const FString Cat = TEXT("Client WorldState");
    OutSnapshot.AddInfo(Cat, TEXT("Initialized"),
        bInitialized ? TEXT("Yes") : TEXT("No"));
    OutSnapshot.AddInfo(Cat, TEXT("LocalFrame"),
        FString::Printf(TEXT("%lld"), LocalFrame));
    OutSnapshot.AddInfo(Cat, TEXT("DiffHistorySize"),
        FString::FromInt(DiffHistory.Num()));
    OutSnapshot.AddInfo(Cat, TEXT("PendingServerBatches"),
        FString::FromInt(PendingServerBatches.Num()));

    if (bInitialized)
    {
        const FHktWorldState& WS = Simulator->GetWorldState();
        OutSnapshot.AddInfo(Cat, TEXT("Entities"),
            FString::FromInt(WS.GetEntityCount()));
        // WorldState 엔티티 리스트/상세는 AdvanceFrame에서 자동 push됨
    }
}
#endif
