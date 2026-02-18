// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGameMode.h"
#include "HktIngamePlayerController.h"
#include "HktPlayerState.h"
#include "HktServerRuleInterfaces.h"
#include "HktRuntimeConverter.h"
#include "Rules/HktServerRule.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHktGameMode, Log, All);

AHktGameMode::AHktGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    // 컴포넌트는 블루프린트나 다른 방식으로 추가됨
}

void AHktGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    if (!ServerRule)
    {
        ServerRule = MakeUnique<FHktDefaultServerRule>();
    }

    // GetComponents를 이용해서 인터페이스들을 캐싱
    TArray<UActorComponent*> Components;
    GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (IHktFrameManager* FrameManager = Cast<IHktFrameManager>(Comp))
        {
            CachedFrameManager = FrameManager;
        }
        else if (IHktRelevancyGraph* RelevancyGraph = Cast<IHktRelevancyGraph>(Comp))
        {
            CachedRelevancyGraph = RelevancyGraph;
        }
        else if (IHktWorldDatabase* WorldDatabase = Cast<IHktWorldDatabase>(Comp))
        {
            CachedWorldDatabase = WorldDatabase;
        }
        else if (IHktIntentCollector* IntentCollector = Cast<IHktIntentCollector>(Comp))
        {
            CachedIntentCollector = IntentCollector;
        }
        else if (IHktBatchBuilder* BatchBuilder = Cast<IHktBatchBuilder>(Comp))
        {
            CachedBatchBuilder = BatchBuilder;
        }
    }

    HKT_INSIGHTS_REGISTER_PROVIDER(this);
}

void AHktGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ServerRule.Reset();
    CachedFrameManager = nullptr;
    CachedRelevancyGraph = nullptr;
    CachedWorldDatabase = nullptr;
    CachedIntentCollector = nullptr;
    CachedBatchBuilder = nullptr;

    HKT_INSIGHTS_UNREGISTER_PROVIDER(this);
    Super::EndPlay(EndPlayReason);
}

void AHktGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

#if WITH_HKT_INSIGHTS
    double TickStart = FPlatformTime::Seconds();
#endif

    IHktServerRule* Rule = GetServerRule();
    if (!Rule)
    {
        UE_LOG(LogHktGameMode, Warning, TEXT("Tick: ServerRule is null"));
        return;
    }

    IHktFrameManager*    Frame       = CachedFrameManager;
    IHktRelevancyGraph*  Graph       = CachedRelevancyGraph;
    IHktIntentCollector* Collector   = CachedIntentCollector;
    IHktBatchBuilder*    Builder     = CachedBatchBuilder;
    IHktWorldDatabase*   Database    = CachedWorldDatabase;

    if (!Frame || !Graph || !Collector || !Builder || !Database)
    {
        UE_LOG(LogHktGameMode, Warning, TEXT("Tick: Required component missing (Frame=%d Graph=%d Collector=%d Builder=%d Database=%d)"),
            Frame != nullptr, Graph != nullptr, Collector != nullptr, Builder != nullptr, Database != nullptr);
        return;
    }

    if (!Frame->IsInitialized())
    {
        UE_LOG(LogHktGameMode, Verbose, TEXT("Tick: Frame not initialized yet"));
        return;
    }

    Frame->AdvanceFrame();

    Rule->OnTick_ProcessPendingConnections(
        *Graph, *Collector, *Database,
        [Graph](const FHktPlayerRecord& Record) -> IHktWorldPlayer*
        {
            return Graph->GetWorldPlayer(Record.PlayerUid);
        }
    );

    Graph->UpdateRelevancy();

    // 1. 실행
    Rule->OnTick_ProcessSimulationAndPayloads(DeltaSeconds, *Frame, *Graph, *Collector, *Builder);
    
    // 2. 전송 (단일 루프 - Cache Friendly)
    // TArrayView를 통해 유효한 데이터만 순회
    TArrayView<const FHktFrameSendPayload> ValidPayloads = Builder->GetValidPayloads();
    
    for (const FHktFrameSendPayload& Payload : ValidPayloads)
    {
        // 여기서 Payload.TargetActor가 유효한지 체크할 수도 있음 (nullptr 처리 관련)
        if (AHktIngamePlayerController* PC = Cast<AHktIngamePlayerController>(Payload.TargetActor))
        {
            if (Payload.StateToSend)
            {
                PC->Client_ReceiveInitialState(HktRuntimeConverter::ConvertWorldState(*Payload.StateToSend));
            }
            else if (Payload.BatchToSend)
            {
                PC->Client_ReceiveFrameBatch(HktRuntimeConverter::ConvertToBatch(*Payload.BatchToSend));
            }
        }
    }

    // EndFrame 호출
    if (Collector)
    {
        Collector->EndFrame();
    }

#if WITH_HKT_INSIGHTS
    LastTickDurationMs = static_cast<float>((FPlatformTime::Seconds() - TickStart) * 1000.0);
#endif
}

void AHktGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AHktIngamePlayerController* HktPC = Cast<AHktIngamePlayerController>(NewPlayer);
    if (!HktPC) return;

    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    IHktWorldDatabase* Database = CachedWorldDatabase;
    if (!Database) return;

    IHktWorldPlayer* WorldPlayer = HktPC->FindComponentByInterface<IHktWorldPlayer>();
    if (!WorldPlayer) return;

    // 컴포넌트가 자동으로 PlayerState에서 UID를 계산하므로 수동 설정 불필요
    Rule->OnLogin_EnterWorldPlayer(*WorldPlayer, *Database);

    UE_LOG(LogHktGameMode, Log, TEXT("PostLogin PlayerUid=%lld"), WorldPlayer->GetPlayerUid());
}

void AHktGameMode::Logout(AController* Exiting)
{
    AHktIngamePlayerController* HktPC = Cast<AHktIngamePlayerController>(Exiting);
    if (!HktPC) return;

    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    IHktWorldDatabase* Database = CachedWorldDatabase;
    if (!Database) return;

    IHktRelevancyGraph* Graph = CachedRelevancyGraph;
    if (!Graph) return;

    IHktWorldPlayer* WorldPlayer = HktPC->FindComponentByInterface<IHktWorldPlayer>();
    if (!WorldPlayer || !WorldPlayer->IsInitialized()) return;

    const int64 PlayerUid = WorldPlayer->GetPlayerUid();
    UE_LOG(LogHktGameMode, Log, TEXT("Logout PlayerUid=%lld"), PlayerUid);

    Rule->OnLogout_ExitWorldPlayer(*WorldPlayer, *Database);

    Super::Logout(Exiting);
}

void AHktGameMode::PushIntent(int64 PlayerUid, const FHktEvent& Event)
{
    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    IHktRelevancyGraph* Graph = CachedRelevancyGraph;
    if (!Graph) return;

    IHktIntentCollector* Collector = CachedIntentCollector;
    if (!Collector) return;

    IHktWorldPlayer* WorldPlayer = Graph->GetWorldPlayer(PlayerUid);
    if (!WorldPlayer) return;

    Rule->OnReceived_FireIntentEvent(Event, *WorldPlayer, *Collector);
}

IHktServerRule* AHktGameMode::GetServerRule() const
{
    return ServerRule.Get();
}

// ============================================================================
// IHktInsightProvider 구현
// ============================================================================

#if WITH_HKT_INSIGHTS
void AHktGameMode::CollectInsightData(FHktInsightSnapshot& OutSnapshot) const
{
    OutSnapshot.ProviderName = TEXT("GameMode");

    // === Frame 정보 ===
    {
        const FString Cat = TEXT("Frame");
        if (CachedFrameManager)
        {
            const IHktFrameManager* Frame = CachedFrameManager;
            OutSnapshot.AddInfo(Cat, TEXT("Initialized"),
                Frame->IsInitialized() ? TEXT("Yes") : TEXT("No"));
            OutSnapshot.AddInfo(Cat, TEXT("CurrentFrame"),
                FString::Printf(TEXT("%lld"), Frame->GetFrameNumber()));
        }
        else
        {
            OutSnapshot.AddWarning(Cat, TEXT("PersistentFrame"), TEXT("NULL"));
        }
    }

    // === Tick 성능 ===
    {
        const FString Cat = TEXT("Performance");
        OutSnapshot.Add(Cat, TEXT("TickDuration"),
            FString::Printf(TEXT("%.2f ms"), LastTickDurationMs),
            LastTickDurationMs > 16.0f ? 1 : 0); // 16ms 초과 시 Warning
    }

    // === Relevancy / Player 정보 ===
    {
        const FString Cat = TEXT("Relevancy");
        if (CachedRelevancyGraph)
        {
            const IHktRelevancyGraph* Graph = CachedRelevancyGraph;
            int32 NumGroups = Graph->NumRelevancyGroup();
            OutSnapshot.AddInfo(Cat, TEXT("NumGroups"), FString::FromInt(NumGroups));

            int32 TotalPlayers = 0;
            for (int32 i = 0; i < NumGroups; ++i)
            {
                const IHktRelevancyGroup& Group = Graph->GetRelevancyGroup(i);
                int32 PlayerCount = Group.GetPlayerUids().Num();
                TotalPlayers += PlayerCount;

                if (NumGroups > 1 || PlayerCount > 0)
                {
                    OutSnapshot.AddInfo(Cat,
                        FString::Printf(TEXT("Group[%d].Players"), i),
                        FString::FromInt(PlayerCount));

                    // 그룹 내 플레이어 UID 목록
                    const TArray<int64>& Uids = Group.GetPlayerUids();
                    FString UidList;
                    for (int32 j = 0; j < FMath::Min(Uids.Num(), 5); ++j)
                    {
                        if (j > 0) UidList += TEXT(", ");
                        UidList += FString::Printf(TEXT("%lld"), Uids[j]);
                    }
                    if (Uids.Num() > 5)
                    {
                        UidList += FString::Printf(TEXT(" (+%d more)"), Uids.Num() - 5);
                    }
                    if (!UidList.IsEmpty())
                    {
                        OutSnapshot.AddInfo(Cat,
                            FString::Printf(TEXT("Group[%d].UIDs"), i), UidList);
                    }
                }
            }
            OutSnapshot.AddInfo(Cat, TEXT("TotalPlayers"), FString::FromInt(TotalPlayers));
        }
        else
        {
            OutSnapshot.AddError(Cat, TEXT("GridRelevancy"), TEXT("NULL"));
        }
    }

    // === ServerRule 정보 ===
    {
        const FString Cat = TEXT("Rule");
        IHktServerRule* Rule = GetServerRule();
        OutSnapshot.AddInfo(Cat, TEXT("ServerRule"), Rule ? TEXT("Active") : TEXT("None"));
    }

    // === Component 상태 요약 ===
    {
        const FString Cat = TEXT("Components");
        OutSnapshot.AddInfo(Cat, TEXT("PersistentFrame"),
            CachedFrameManager ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("GridRelevancy"),
            CachedRelevancyGraph ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("PlayerDatabase"),
            CachedWorldDatabase ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("IntentCollector"),
            CachedIntentCollector ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("BatchBuilder"),
            CachedBatchBuilder ? TEXT("OK") : TEXT("NULL"));
    }
}
#endif

