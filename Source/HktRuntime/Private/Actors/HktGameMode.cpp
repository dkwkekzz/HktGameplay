// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGameMode.h"
#include "HktIngamePlayerController.h"
#include "HktPlayerState.h"
#include "HktServerRuleInterfaces.h"
#include "HktClientRuleInterfaces.h"
#include "HktRuntimeConverter.h"
#include "HktRuntimeTypes.h"

#if WITH_HKT_INSIGHTS
#include "HktRuntimeInsightsCollector.h"
#include "HktCoreSimulator.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHktGameMode, Log, All);

AHktGameMode::AHktGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
}

void AHktGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    CachedServerRule = HktRule::GetServerRule(GetWorld());
    if (!CachedServerRule) 
    {
        UE_LOG(LogHktGameMode, Error, TEXT("InitGame: ServerRule is null"));
        return;
    }

    // 컴포넌트에서 인터페이스 캐싱
    TArray<UActorComponent*> Components;
    GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (IHktFrameManager* FM = Cast<IHktFrameManager>(Comp))
        {
            CachedFrameManager = FM;
        }
        else if (IHktRelevancyGraph* RG = Cast<IHktRelevancyGraph>(Comp))
        {
            CachedRelevancyGraph = RG;
        }
        else if (IHktWorldDatabase* WD = Cast<IHktWorldDatabase>(Comp))
        {
            CachedWorldDatabase = WD;
        }
    }

    // Rule에 컨텍스트 바인딩 (item 2)
    if (CachedServerRule)
    {
        CachedServerRule->BindContext(
            CachedFrameManager,
            CachedRelevancyGraph,
            CachedWorldDatabase);
    }

    HKT_INSIGHTS_REGISTER_PROVIDER(this);
}

void AHktGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CachedServerRule)
    {
        CachedServerRule->BindContext(nullptr, nullptr, nullptr);
    }

    CachedServerRule             = nullptr;
    CachedFrameManager           = nullptr;
    CachedRelevancyGraph         = nullptr;
    CachedWorldDatabase          = nullptr;

    HKT_INSIGHTS_UNREGISTER_PROVIDER(this);
    Super::EndPlay(EndPlayReason);
}

void AHktGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!CachedFrameManager || !CachedFrameManager->IsInitialized())
    {
        UE_LOG(LogHktGameMode, Verbose, TEXT("Tick: Frame not initialized yet"));
        return;
    }

    // 고정 간격 시뮬레이션 (결정론적: 서버도 1/30초 고정 틱)
    FrameAccumulator += DeltaSeconds;
    while (FrameAccumulator >= FixedDeltaTime)
    {
        FrameAccumulator -= FixedDeltaTime;
        SimulationTick();
    }
}

void AHktGameMode::SimulationTick()
{
#if WITH_HKT_INSIGHTS
    double TickStart = FPlatformTime::Seconds();
#endif

    IHktServerRule* Rule = GetServerRule();
    if (!Rule)
    {
        UE_LOG(LogHktGameMode, Warning, TEXT("Tick: ServerRule is null"));
        return;
    }

    const FHktEventGameModeTickResult TickResult = Rule->OnEvent_GameModeTick(FixedDeltaTime);

    for (const FGroupEventSend& GroupSend : TickResult.EventSends)
    {
        const FHktSimulationEvent& Batch = GroupSend.Batch;
        const bool bHasContent = Batch.NewEvents.Num() > 0
            || Batch.NewEntityStates.Num() > 0
            || Batch.RemovedOwnerIds.Num() > 0;

        if (bHasContent)
        {
            const TArray<IHktWorldPlayer*>& Existing = *GroupSend.Existing;
            for (IHktWorldPlayer* Player : Existing)
            {
                if (AHktIngamePlayerController* PC = Cast<AHktIngamePlayerController>(Player->GetOwnerActor()))
                {
                    PC->Client_ReceiveFrameBatch(HktRuntimeConverter::ConvertToBatch(Batch));
                }
            }
        }

        const TArray<IHktWorldPlayer*>& Entered = GroupSend.Entered;
        for (IHktWorldPlayer* Newbie : Entered)
        {
            if (AHktIngamePlayerController* PC = Cast<AHktIngamePlayerController>(Newbie->GetOwnerActor()))
            {
                PC->Client_ReceiveInitialState(HktRuntimeConverter::ConvertWorldState(*GroupSend.NewState));
            }
        }
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

    IHktWorldPlayer* WorldPlayer = HktPC->FindComponentByInterface<IHktWorldPlayer>();
    if (!WorldPlayer) return;

    // item 1: 액터 이벤트 그대로 전달 (DB 파라미터 없음 — item 2)
    Rule->OnEvent_GameModePostLogin(*WorldPlayer);

    UE_LOG(LogHktGameMode, Log, TEXT("PostLogin PlayerUid=%lld"), WorldPlayer->GetPlayerUid());
}

void AHktGameMode::Logout(AController* Exiting)
{
    AHktIngamePlayerController* HktPC = Cast<AHktIngamePlayerController>(Exiting);
    if (!HktPC) return;

    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    IHktWorldPlayer* WorldPlayer = HktPC->FindComponentByInterface<IHktWorldPlayer>();
    if (!WorldPlayer || !WorldPlayer->IsInitialized()) return;

    const int64 PlayerUid = WorldPlayer->GetPlayerUid();
    UE_LOG(LogHktGameMode, Log, TEXT("Logout PlayerUid=%lld"), PlayerUid);

    // item 1: 액터 이벤트 그대로 전달 (DB 파라미터 없음 — item 2)
    Rule->OnEvent_GameModeLogout(*WorldPlayer);

    Super::Logout(Exiting);
}

void AHktGameMode::PushIntent(int64 PlayerUid, const FHktEvent& Event)
{
    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    IHktRelevancyGraph* Graph = CachedRelevancyGraph;
    if (!Graph) return;

    IHktWorldPlayer* WorldPlayer = Graph->GetWorldPlayer(PlayerUid);
    if (!WorldPlayer) return;

    // item 2: Graph/Builder 파라미터 없음 — Rule이 내부 캐싱된 컨텍스트 사용
    Rule->OnReceived_FireIntentEvent(Event, *WorldPlayer);
}

IHktServerRule* AHktGameMode::GetServerRule() const
{
    return CachedServerRule;
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
            OutSnapshot.AddInfo(Cat, TEXT("Initialized"),
                CachedFrameManager->IsInitialized() ? TEXT("Yes") : TEXT("No"));
            OutSnapshot.AddInfo(Cat, TEXT("CurrentFrame"),
                FString::Printf(TEXT("%lld"), CachedFrameManager->GetFrameNumber()));
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
            LastTickDurationMs > 16.0f ? 1 : 0);
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
        OutSnapshot.AddInfo(Cat, TEXT("ServerRule"), CachedServerRule ? TEXT("Active") : TEXT("None"));
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
    }
}
#endif
