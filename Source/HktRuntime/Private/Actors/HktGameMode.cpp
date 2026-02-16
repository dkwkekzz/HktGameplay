// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGameMode.h"
#include "HktInGamePlayerController.h"
#include "HktPlayerState.h"
#include "HktServerRuleInterfaces.h"
#include "HktRuntimeConverter.h"
#include "Rules/HktServerRule.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#endif

AHktGameMode::AHktGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    // 컴포넌트는 블루프린트나 다른 방식으로 추가됨
}

void AHktGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (!ServerRule)
    {
        ServerRule = MakeUnique<FHktDefaultServerRule>();
    }

    // FindComponentByInterface를 이용해서 인터페이스들을 캐싱
    if (IHktFrameManager* FrameManager = FindComponentByInterface<IHktFrameManager>())
    {
        CachedFrameManager = TScriptInterface<IHktFrameManager>(FrameManager);
    }
    if (IHktRelevancyGraph* RelevancyGraph = FindComponentByInterface<IHktRelevancyGraph>())
    {
        CachedRelevancyGraph = TScriptInterface<IHktRelevancyGraph>(RelevancyGraph);
    }
    if (IHktWorldDatabase* WorldDatabase = FindComponentByInterface<IHktWorldDatabase>())
    {
        CachedWorldDatabase = TScriptInterface<IHktWorldDatabase>(WorldDatabase);
    }
    if (IHktIntentCollector* IntentCollector = FindComponentByInterface<IHktIntentCollector>())
    {
        CachedIntentCollector = TScriptInterface<IHktIntentCollector>(IntentCollector);
    }
    if (IHktBatchBuilder* BatchBuilder = FindComponentByInterface<IHktBatchBuilder>())
    {
        CachedBatchBuilder = TScriptInterface<IHktBatchBuilder>(BatchBuilder);
    }

    HKT_INSIGHTS_REGISTER_PROVIDER(this);

    UE_LOG(LogTemp, Log, TEXT("[HktGameMode] BeginPlay"));
}

void AHktGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
    if (!Rule) return;

    IHktFrameManager*    Frame       = CachedFrameManager.GetInterface();
    IHktRelevancyGraph*  Graph       = CachedRelevancyGraph.GetInterface();
    IHktIntentCollector* Collector   = CachedIntentCollector.GetInterface();
    IHktBatchBuilder*    Builder     = CachedBatchBuilder.GetInterface();
    IHktWorldDatabase*   Database    = CachedWorldDatabase.GetInterface();

    if (!Frame || !Graph || !Collector || !Builder || !Database) return;
    if (!Frame->IsInitialized()) return;

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
        if (AHktInGamePlayerController* PC = Cast<AHktInGamePlayerController>(Payload.TargetActor))
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

    AHktInGamePlayerController* HktPC = Cast<AHktInGamePlayerController>(NewPlayer);
    if (!HktPC) return;

    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    TScriptInterface<IHktWorldPlayer> WorldPlayerInterface;
    if (IHktWorldPlayer* WorldPlayer = HktPC->FindComponentByInterface<IHktWorldPlayer>())
    {
        WorldPlayerInterface = TScriptInterface<IHktWorldPlayer>(WorldPlayer);
    }
    if (!WorldPlayerInterface.GetInterface()) return;

    IHktWorldPlayer* WorldPlayer = WorldPlayerInterface.GetInterface();
    // 컴포넌트가 자동으로 PlayerState에서 UID를 계산하므로 수동 설정 불필요
    int64 PlayerUid = WorldPlayer->GetPlayerUid();

    IHktWorldDatabase* Database = CachedWorldDatabase.GetInterface();
    if (Database)
    {
        Rule->OnLogin_EnterWorldPlayer(*WorldPlayer, *Database);
    }

    UE_LOG(LogTemp, Log, TEXT("[HktGameMode] PostLogin: PlayerUid=%lld"), PlayerUid);
}

void AHktGameMode::Logout(AController* Exiting)
{
    AHktInGamePlayerController* HktPC = Cast<AHktInGamePlayerController>(Exiting);
    if (HktPC)
    {
        IHktServerRule* Rule = GetServerRule();
        TScriptInterface<IHktWorldPlayer> WorldPlayerInterface;
    if (IHktWorldPlayer* WorldPlayer = HktPC->FindComponentByInterface<IHktWorldPlayer>())
    {
        WorldPlayerInterface = TScriptInterface<IHktWorldPlayer>(WorldPlayer);
    }
        IHktWorldPlayer* WorldPlayer = WorldPlayerInterface.GetInterface();

        if (Rule && WorldPlayer && WorldPlayer->IsInitialized())
        {
            IHktWorldDatabase* Database = CachedWorldDatabase.GetInterface();
            if (Database)
            {
                Rule->OnLogout_ExitWorldPlayer(*WorldPlayer, *Database);
            }

            IHktRelevancyGraph* Graph = CachedRelevancyGraph.GetInterface();
            if (Graph)
            {
                Graph->UnregisterPlayer(WorldPlayer->GetPlayerUid());
            }
        }
    }

    Super::Logout(Exiting);
}

void AHktGameMode::PushIntent(int64 PlayerUid, const FHktEvent& Event)
{
    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    IHktWorldPlayer* WorldPlayer = nullptr;
    IHktRelevancyGraph* Graph = CachedRelevancyGraph.GetInterface();
    if (Graph)
    {
        WorldPlayer = Graph->GetWorldPlayer(PlayerUid);
    }
    IHktIntentCollector* Collector = CachedIntentCollector.GetInterface();

    if (WorldPlayer && Collector)
    {
        Rule->OnReceived_FireIntentEvent(Event, *WorldPlayer, *Collector);
    }
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
        if (CachedFrameManager.GetInterface())
        {
            const IHktFrameManager* Frame = CachedFrameManager.GetInterface();
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
        if (CachedRelevancyGraph.GetInterface())
        {
            const IHktRelevancyGraph* Graph = CachedRelevancyGraph.GetInterface();
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
            CachedFrameManager.GetInterface() ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("GridRelevancy"),
            CachedRelevancyGraph.GetInterface() ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("PlayerDatabase"),
            CachedWorldDatabase.GetInterface() ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("IntentCollector"),
            CachedIntentCollector.GetInterface() ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("BatchBuilder"),
            CachedBatchBuilder.GetInterface() ? TEXT("OK") : TEXT("NULL"));
    }
}
#endif

