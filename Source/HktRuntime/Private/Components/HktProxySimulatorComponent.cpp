#include "HktProxySimulatorComponent.h"
#include "HktRuntimeCommon.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#include "HktWorldStateInsightsHelper.h"
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
    Simulator = CreateDeterminismSimulator();
    HKT_INSIGHTS_REGISTER_PROVIDER(this);
}

void UHktProxySimulatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    HKT_INSIGHTS_UNREGISTER_PROVIDER(this);
    Simulator.Reset();
    Super::EndPlay(EndPlayReason);
}

// ============================================================================
// 매 틱: 고정 타임스텝으로 로컬 시뮬레이션
// ============================================================================

void UHktProxySimulatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bInitialized) return;

    FrameAccumulator += DeltaTime;
    while (FrameAccumulator >= FixedDeltaTime)
    {
        FrameAccumulator -= FixedDeltaTime;
        AdvanceLocalFrame(FixedDeltaTime);
    }
}

void UHktProxySimulatorComponent::AdvanceLocalFrame(float DeltaSeconds)
{
    LocalFrame++;

    FHktSimulationEvent LocalBatch = BuildLocalBatch(LocalFrame, DeltaSeconds);
    Simulator->AdvanceFrame(LocalBatch);

    // 히스토리에 기록 (롤백용)
    LocalHistory.Add(MoveTemp(LocalBatch));

    // 메모리 보호: 오래된 히스토리 제거
    if (LocalHistory.Num() > MaxHistoryFrames)
    {
        // 스냅샷을 현재 상태로 갱신하고 히스토리 비움
        // (서버 확정 없이 너무 오래 지나면 스냅샷 갱신)
        SnapshotState.CopyFrom(Simulator->GetWorldState());
        SnapshotFrame = LocalFrame;
        LocalHistory.Empty();
    }
}

FHktSimulationEvent UHktProxySimulatorComponent::BuildLocalBatch(
    int64 Frame, float DeltaSeconds) const
{
    FHktSimulationEvent Batch;
    Batch.FrameNumber = Frame;
    Batch.DeltaSeconds = DeltaSeconds;
    // 서버와 동일한 시드 생성 규칙 사용
    Batch.RandomSeed = HktRuntimeCommon::HashCombineHelper(Frame, 0);
    return Batch;
}

// ============================================================================
// 서버 Batch 수신 → 롤백 & 재시뮬레이션
// ============================================================================

FHktSimulationDiff UHktProxySimulatorComponent::ReconcileWithServerBatch(const FHktSimulationEvent& InBatch)
{
    if (!bInitialized) return FHktSimulationDiff();

    const int64 ServerFrame = InBatch.FrameNumber;

    // 1. 스냅샷으로 롤백
    Simulator->RestoreWorldState(SnapshotState);

    // 2. 스냅샷 ~ 서버 프레임 사이의 로컬 히스토리 재실행
    for (const FHktSimulationEvent& H : LocalHistory)
    {
        if (H.FrameNumber >= ServerFrame) break;
        Simulator->AdvanceFrame(H);
    }

    // 3. 서버 Batch로 해당 프레임 실행 — 이 프레임의 Diff가 프레젠테이션에 전달됨
    FHktSimulationDiff ServerDiff = Simulator->AdvanceFrame(InBatch);

    // 4. 새로운 스냅샷 저장 (서버 확정 프레임)
    SnapshotState.CopyFrom(Simulator->GetWorldState());
    SnapshotFrame = ServerFrame;

    // 5. 서버 프레임 이후의 로컬 히스토리 재실행
    TArray<FHktSimulationEvent> RemainingHistory;
    for (const FHktSimulationEvent& H : LocalHistory)
    {
        if (H.FrameNumber > ServerFrame)
        {
            Simulator->AdvanceFrame(H);
            RemainingHistory.Add(H);
        }
    }
    LocalHistory = MoveTemp(RemainingHistory);

    // 6. 로컬 프레임이 서버보다 뒤처져 있으면 따라잡기
    if (LocalFrame < ServerFrame)
    {
        LocalFrame = ServerFrame;
    }

    // 7. Accum 초기화 — 서버 Batch 수신 시 누적 시간 리셋
    FrameAccumulator = 0.0f;

    return ServerDiff;
}

// ============================================================================
// InitialState 수신 (그룹 진입 시)
// ============================================================================

void UHktProxySimulatorComponent::RestoreState(const FHktWorldState& InState)
{
    Simulator->RestoreWorldState(InState);

    SnapshotState.CopyFrom(InState);
    SnapshotFrame = InState.FrameNumber;
    LocalFrame = InState.FrameNumber;
    LocalHistory.Empty();
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
    OutSnapshot.AddInfo(Cat, TEXT("SnapshotFrame"),
        FString::Printf(TEXT("%lld"), SnapshotFrame));
    OutSnapshot.AddInfo(Cat, TEXT("HistorySize"),
        FString::FromInt(LocalHistory.Num()));

    if (bInitialized)
    {
        const FHktWorldState& WS = Simulator->GetWorldState();
        OutSnapshot.AddInfo(Cat, TEXT("Entities"),
            FString::FromInt(WS.GetEntityCount()));

        if (WS.GetEntityCount() > 0)
        {
            FHktWorldStateSnapshot Snapshot =
                HktWorldStateInsights::BuildSnapshot(WS, TEXT("Client"));
            FHktRuntimeInsightsCollector::Get().PushWorldStateSnapshot(
                MoveTemp(Snapshot));
        }
    }
}
#endif