// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktGameMode.h"
#include "HktInGamePlayerController.h"
#include "HktRuleSubsystem.h"
#include "HktRuleInterfaces.h"
#include "Rules/HktServerRule.h"
#include "Components/HktMasterStashComponent.h"
#include "Components/HktGridRelevancyComponent.h"
#include "Components/HktVMProcessorComponent.h"
#include "Components/HktPlayerDatabaseComponent.h"
#include "Components/HktPersistentFrameComponent.h"
#include "Components/HktIntentCollectorComponent.h"
#include "Components/HktBatchBuilderComponent.h"

AHktGameMode::AHktGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    // 인터페이스 구현 컴포넌트 생성
    PersistentFrameComponent  = CreateDefaultSubobject<UHktPersistentFrameComponent>(TEXT("PersistentFrame"));
    MasterStashComponent      = CreateDefaultSubobject<UHktMasterStashComponent>(TEXT("MasterStash"));
    GridRelevancyComponent    = CreateDefaultSubobject<UHktGridRelevancyComponent>(TEXT("GridRelevancy"));
    PlayerDatabaseComponent   = CreateDefaultSubobject<UHktPlayerDatabaseComponent>(TEXT("PlayerDatabase"));
    IntentCollectorComponent  = CreateDefaultSubobject<UHktIntentCollectorComponent>(TEXT("IntentCollector"));
    BatchBuilderComponent     = CreateDefaultSubobject<UHktBatchBuilderComponent>(TEXT("BatchBuilder"));
    VMProcessorComponent      = CreateDefaultSubobject<UHktVMProcessorComponent>(TEXT("VMProcessor"));
}

void AHktGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 기본 ServerRule 등록 (아직 없다면)
    if (UHktRuleSubsystem* RuleSS = UHktRuleSubsystem::Get(GetWorld()))
    {
        if (!RuleSS->GetServerRule())
        {
            RuleSS->SetServerRule(MakeShared<FHktDefaultServerRule>());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[HktGameMode] BeginPlay - ServerRule connected"));
}

// ============================================================================
// Tick → Rule 이벤트 발행
// ============================================================================

void AHktGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    // 컴포넌트 → 인터페이스 획득
    IHktPersistentFrame* Frame       = PersistentFrameComponent;  // UHktPersistentFrameComponent : public IHktPersistentFrame
    IHktRelevancyGraph*  Graph       = GridRelevancyComponent;    // UHktGridRelevancyComponent : public IHktRelevancyGraph  
    IHktIntentCollector* Collector   = IntentCollectorComponent;  // UHktIntentCollectorComponent : public IHktIntentCollector
    IHktBatchBuilder*    Builder     = BatchBuilderComponent;     // UHktBatchBuilderComponent : public IHktBatchBuilder
    IHktWorldDatabase*   Database    = PlayerDatabaseComponent;   // UHktPlayerDatabaseComponent : public IHktWorldDatabase

    if (!Frame || !Graph || !Collector || !Builder || !Database) return;
    if (!Frame->IsInitialized()) return;

    // Phase 1: 프레임 진행
    Frame->AdvanceFrame();

    // Phase 2: 대기 중인 로그인/로그아웃 처리
    Rule->OnTick_ProcessPendingConnections(
        *Graph, *Collector, *Database,
        [this](const FHktPlayerRecord& Record) -> IHktWorldPlayer*
        {
            // PlayerFactory: Record → IHktWorldPlayer*
            // 실제 구현에서는 PlayerController를 찾아 IHktWorldPlayer 어댑터를 반환
            // (GridRelevancyComponent가 관리하는 WorldPlayer 래퍼)
            return GridRelevancyComponent->CreateWorldPlayerFromRecord(Record);
        }
    );

    // Phase 3: Relevancy 업데이트
    Graph->UpdateRelevancy();

    // Phase 4: 시뮬레이션 실행 (ParallelFor 내부)
    Builder->Reset(Graph->NumRelevancyGroup());
    Rule->OnTick_ExecuteFrame(*Frame, *Graph, *Collector, *Builder);

    // Phase 5: 결과 전송
    Rule->OnTick_SendFrameBatch(*Graph, *Builder);

    // Phase 6: Collector 프레임 종료 (처리된 Intent 정리)
    Collector->EndFrame();
}

// ============================================================================
// PostLogin → Rule 이벤트 발행
// ============================================================================

void AHktGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AHktInGamePlayerController* HktPC = Cast<AHktInGamePlayerController>(NewPlayer);
    if (!HktPC) return;

    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    // GridRelevancyComponent가 PlayerController를 WorldPlayer 어댑터로 래핑
    IHktWorldPlayer* WorldPlayer = GridRelevancyComponent->RegisterAndWrapClient(HktPC);
    if (!WorldPlayer) return;

    // Rule에 로그인 이벤트 발행 (Rule이 DB 로드를 결정)
    IHktWorldDatabase* Database = PlayerDatabaseComponent;
    if (Database)
    {
        Rule->OnLogin_EnterWorldPlayer(*WorldPlayer, *Database);
    }

    UE_LOG(LogTemp, Log, TEXT("[HktGameMode] PostLogin → Rule->OnLogin: PlayerUid=%lld"), WorldPlayer->GetPlayerUid());
}

// ============================================================================
// Logout → Rule 이벤트 발행
// ============================================================================

void AHktGameMode::Logout(AController* Exiting)
{
    AHktInGamePlayerController* HktPC = Cast<AHktInGamePlayerController>(Exiting);
    if (HktPC)
    {
        IHktServerRule* Rule = GetServerRule();
        IHktWorldPlayer* WorldPlayer = GridRelevancyComponent->FindWorldPlayer(HktPC);

        if (Rule && WorldPlayer)
        {
            IHktWorldDatabase* Database = PlayerDatabaseComponent;
            if (Database)
            {
                Rule->OnLogout_ExitWorldPlayer(*WorldPlayer, *Database);
            }
        }

        GridRelevancyComponent->UnregisterClient(HktPC);
    }

    Super::Logout(Exiting);
}

// ============================================================================
// Intent 수신 (PlayerController → GameMode → Rule)
// ============================================================================

void AHktGameMode::PushIntent(int64 PlayerUid, const FHktIntentEvent& Event)
{
    IHktServerRule* Rule = GetServerRule();
    if (!Rule) return;

    IHktWorldPlayer* WorldPlayer = GridRelevancyComponent->FindWorldPlayerByUid(PlayerUid);
    IHktIntentCollector* Collector = IntentCollectorComponent;

    if (WorldPlayer && Collector)
    {
        Rule->OnReceived_FireIntentEvent(Event, *WorldPlayer, *Collector);
    }
}

// ============================================================================
// Helpers
// ============================================================================

IHktServerRule* AHktGameMode::GetServerRule() const
{
    if (UHktRuleSubsystem* RuleSS = UHktRuleSubsystem::Get(GetWorld()))
    {
        TSharedPtr<IHktServerRule> Rule = RuleSS->GetServerRule();
        return Rule.Get();
    }
    return nullptr;
}
