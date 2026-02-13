// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGameMode.h"
#include "HktInGamePlayerController.h"
#include "HktPlayerState.h"
#include "HktRuleSubsystem.h"
#include "HktRuleInterfaces.h"
#include "HktRuntimeConverter.h"
#include "Rules/HktServerRule.h"
#include "Components/HktGridRelevancyComponent.h"
#include "Components/HktFileDatabaseComponent.h"
#include "Components/HktFilePersistentFrameComponent.h"
#include "Components/HktIntentCollectorComponent.h"
#include "Components/HktBatchBuilderComponent.h"
#include "Components/HktWorldPlayerComponent.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#endif

AHktGameMode::AHktGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    PersistentFrameComponent  = CreateDefaultSubobject<UHktFilePersistentFrameComponent>(TEXT("PersistentFrame"));
    GridRelevancyComponent    = CreateDefaultSubobject<UHktGridRelevancyComponent>(TEXT("GridRelevancy"));
    PlayerDatabaseComponent   = CreateDefaultSubobject<UHktFileDatabaseComponent>(TEXT("PlayerDatabase"));
    IntentCollectorComponent  = CreateDefaultSubobject<UHktIntentCollectorComponent>(TEXT("IntentCollector"));
    BatchBuilderComponent     = CreateDefaultSubobject<UHktBatchBuilderComponent>(TEXT("BatchBuilder"));
}

void AHktGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (UHktRuleSubsystem* RuleSS = UHktRuleSubsystem::Get(GetWorld()))
    {
        if (!RuleSS->GetServerRule())
        {
            RuleSS->SetServerRule(MakeShared<FHktDefaultServerRule>());
        }
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

    IHktFrameManager*    Frame       = PersistentFrameComponent;
    IHktRelevancyGraph*  Graph       = GridRelevancyComponent;
    IHktIntentCollector* Collector   = IntentCollectorComponent;
    IHktBatchBuilder*    Builder     = BatchBuilderComponent;
    IHktWorldDatabase*   Database    = PlayerDatabaseComponent;

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
    TArrayView<const FHktFrameSendPayload> ValidPayloads = BatchBuilderComponent->GetValidPayloads();
    
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

    IntentCollectorComponent->EndFrame();

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

    UHktWorldPlayerComponent* WorldPlayerComp = HktPC->FindComponentByClass<UHktWorldPlayerComponent>();
    if (!WorldPlayerComp) return;

    int64 PlayerUid = HktPC->GetPlayerUid();
    WorldPlayerComp->SetPlayerUid(PlayerUid);

    IHktWorldDatabase* Database = PlayerDatabaseComponent;
    if (Database)
    {
        Rule->OnLogin_EnterWorldPlayer(*WorldPlayerComp, *Database);
    }

    UE_LOG(LogTemp, Log, TEXT("[HktGameMode] PostLogin: PlayerUid=%lld"), PlayerUid);
}

void AHktGameMode::Logout(AController* Exiting)
{
    AHktInGamePlayerController* HktPC = Cast<AHktInGamePlayerController>(Exiting);
    if (HktPC)
    {
        IHktServerRule* Rule = GetServerRule();
        UHktWorldPlayerComponent* WorldPlayerComp = HktPC->FindComponentByClass<UHktWorldPlayerComponent>();

        if (Rule && WorldPlayerComp && WorldPlayerComp->IsInitialized())
        {
            IHktWorldDatabase* Database = PlayerDatabaseComponent;
            if (Database)
            {
                Rule->OnLogout_ExitWorldPlayer(*WorldPlayerComp, *Database);
            }

            IHktRelevancyGraph* Graph = GridRelevancyComponent;
            if (Graph)
            {
                Graph->UnregisterPlayer(WorldPlayerComp->GetPlayerUid());
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
    IHktRelevancyGraph* Graph = GridRelevancyComponent;
    if (Graph)
    {
        WorldPlayer = Graph->GetWorldPlayer(PlayerUid);
    }
    IHktIntentCollector* Collector = IntentCollectorComponent;

    if (WorldPlayer && Collector)
    {
        Rule->OnReceived_FireIntentEvent(Event, *WorldPlayer, *Collector);
    }
}

IHktServerRule* AHktGameMode::GetServerRule() const
{
    if (UHktRuleSubsystem* RuleSS = UHktRuleSubsystem::Get(GetWorld()))
    {
        TSharedPtr<IHktServerRule> Rule = RuleSS->GetServerRule();
        return Rule.Get();
    }
    return nullptr;
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
        if (PersistentFrameComponent)
        {
            OutSnapshot.AddInfo(Cat, TEXT("Initialized"),
                PersistentFrameComponent->IsInitialized() ? TEXT("Yes") : TEXT("No"));
            OutSnapshot.AddInfo(Cat, TEXT("CurrentFrame"),
                FString::Printf(TEXT("%lld"), PersistentFrameComponent->GetFrameNumber()));
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
        if (GridRelevancyComponent)
        {
            const IHktRelevancyGraph* Graph = GridRelevancyComponent;
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
            PersistentFrameComponent ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("GridRelevancy"),
            GridRelevancyComponent ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("PlayerDatabase"),
            PlayerDatabaseComponent ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("IntentCollector"),
            IntentCollectorComponent ? TEXT("OK") : TEXT("NULL"));
        OutSnapshot.AddInfo(Cat, TEXT("BatchBuilder"),
            BatchBuilderComponent ? TEXT("OK") : TEXT("NULL"));
    }
}
#endif

