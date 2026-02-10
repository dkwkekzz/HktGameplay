// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGameMode.h"
#include "HktInGamePlayerController.h"
#include "HktPlayerState.h"
#include "HktRuleSubsystem.h"
#include "HktRuleInterfaces.h"
#include "Rules/HktServerRule.h"
#include "Components/HktGridRelevancyComponent.h"
#include "Components/HktFileDatabaseComponent.h"
#include "Components/HktFilePersistentFrameComponent.h"
#include "Components/HktIntentCollectorComponent.h"
#include "Components/HktBatchBuilderComponent.h"
#include "Components/HktWorldPlayerComponent.h"

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

    UE_LOG(LogTemp, Log, TEXT("[HktGameMode] BeginPlay"));
}

void AHktGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

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

    BatchBuilderComponent->Reset(Graph->NumRelevancyGroup());
    Rule->OnTick_ExecuteFrame(DeltaSeconds, *Frame, *Graph, *Collector, *Builder);

    Rule->OnTick_SendFrameBatch(*Graph, *Builder);

    IntentCollectorComponent->EndFrame();
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

    int64 PlayerUid = 0;
    if (HktPC->PlayerState)
    {
        FUniqueNetIdRepl UniqueId = HktPC->PlayerState->GetUniqueId();
        if (UniqueId.IsValid())
        {
            PlayerUid = GetTypeHash(UniqueId->ToString());
        }
    }
    if (PlayerUid == 0)
    {
        PlayerUid = GetTypeHash(HktPC->GetName());
    }
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

void AHktGameMode::PushIntent(int64 PlayerUid, const FHktIntentEvent& Event)
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
