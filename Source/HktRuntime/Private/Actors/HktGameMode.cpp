#include "HktGameMode.h"
#include "HktPlayerController.h"
#include "HktPlayerState.h"
#include "Components/HktMasterStashComponent.h"
#include "Components/HktGridRelevancyComponent.h"
#include "Components/HktVMProcessorComponent.h"
#include "Components/HktPlayerDatabaseComponent.h"
#include "Components/HktPersistentTickComponent.h"
#include "HktCoreInterfaces.h"
#include "HktPropertyIds.h"
#include "Async/ParallelFor.h"

#if WITH_HKT_INSIGHTS
#include "HktInsightsDataCollector.h"
#endif

AHktGameMode::AHktGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    PersistentTick = CreateDefaultSubobject<UHktPersistentTickComponent>(TEXT("PersistentTick"));
    MasterStash = CreateDefaultSubobject<UHktMasterStashComponent>(TEXT("MasterStash"));
    GridRelevancy = CreateDefaultSubobject<UHktGridRelevancyComponent>(TEXT("GridRelevancy"));
    VMProcessor = CreateDefaultSubobject<UHktVMProcessorComponent>(TEXT("VMProcessor"));
    PlayerDatabase = CreateDefaultSubobject<UHktPlayerDatabaseComponent>(TEXT("PlayerDatabase"));
}

void AHktGameMode::BeginPlay()
{
    Super::BeginPlay();

    // VMProcessor를 MasterStash와 연결
    if (VMProcessor && MasterStash)
    {
        VMProcessor->Initialize(MasterStash->GetStashInterface());
    }

    // GridRelevancy에 MasterStash 연결 (엔티티 위치 조회용)
    if (GridRelevancy && MasterStash)
    {
        GridRelevancy->SetMasterStash(MasterStash);
    }

    UE_LOG(LogTemp, Log, TEXT("HktGameMode: Initialized"));
}

int32 AHktGameMode::GetFrameNumber() const
{
    if (PersistentTick && PersistentTick->IsInitialized())
    {
        return static_cast<int32>(PersistentTick->GetCurrentPersistentFrame());
    }
    return 0;
}

void AHktGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (PersistentTick && PersistentTick->AdvanceFrame() >= 0)
    {
        ProcessFrame();
    }
}

void AHktGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AHktPlayerController* HktPC = Cast<AHktPlayerController>(NewPlayer);
    if (!HktPC) return;

    FString PlayerId = GetPlayerId(NewPlayer);
    
    if (GridRelevancy)
    {
        GridRelevancy->RegisterClient(HktPC);
    }
    
    if (PlayerDatabase)
    {
        PlayerDatabase->GetOrCreatePlayerRecord(PlayerId, [this, HktPC](FHktPlayerRecord& Record)
        {
            LoadPlayerEntities(HktPC, Record);
        });
    }
    
    UE_LOG(LogTemp, Log, TEXT("HktGameMode: Player logged in - %s"), *PlayerId);
}

void AHktGameMode::Logout(AController* Exiting)
{
    AHktPlayerController* HktPC = Cast<AHktPlayerController>(Exiting);
    if (HktPC)
    {
        SavePlayerEntities(HktPC);
        
        if (GridRelevancy)
        {
            GridRelevancy->UnregisterClient(HktPC);
        }
        
        if (PlayerDatabase)
        {
            FString PlayerId = GetPlayerId(HktPC);
            TArray<FHktEntityId> RuntimeIds = PlayerDatabase->GetPlayerRuntimeIds(PlayerId);
            IHktStashInterface* Stash = GetStashInterface();
            if (Stash)
            {
                for (FHktEntityId RuntimeId : RuntimeIds)
                {
                    Stash->FreeEntity(RuntimeId);
                }
            }
            PlayerDatabase->ClearPlayerMappings(PlayerId);
        }
    }
    Super::Logout(Exiting);
}

FString AHktGameMode::GetPlayerId(APlayerController* PC) const
{
    if (!PC) return FString();
    
    if (PC->PlayerState)
    {
        FUniqueNetIdRepl UniqueId = PC->PlayerState->GetUniqueId();
        if (UniqueId.IsValid())
        {
            return UniqueId->ToString();
        }
    }
    
    return FString::Printf(TEXT("Local_%s"), *PC->GetName());
}

int32 AHktGameMode::GenerateEventId()
{
    return NextEventId++;
}

void AHktGameMode::LoadPlayerEntities(AHktPlayerController* PC, FHktPlayerRecord& Record)
{
    if (!MasterStash || !PlayerDatabase || !PC)
    {
        return;
    }
    
    IHktStashInterface* Stash = MasterStash->GetStashInterface();
    if (!Stash)
    {
        return;
    }
    
    FString PlayerId = GetPlayerId(PC);
    int32 PlayerHash = GetTypeHash(PlayerId);
    
    for (int32 i = 0; i < Record.OwnedEntities.Num(); ++i)
    {
        FHktEntityRecord& EntityRecord = Record.OwnedEntities[i];
        
        FHktEntityId RuntimeId = Stash->AllocateEntity();
        if (RuntimeId == InvalidEntityId)
        {
            UE_LOG(LogTemp, Error, TEXT("HktGameMode: Failed to allocate entity"));
            continue;
        }
        
        const int32 PropSize = EntityRecord.Properties.Num();
        for (int32 PropId = 0; PropSize; ++PropId)
        {
            Stash->SetProperty(RuntimeId, PropId, EntityRecord.Properties[PropId]);
        }
        
        Stash->SetTags(RuntimeId, EntityRecord.Tags);
        Stash->SetProperty(RuntimeId, PropertyId::OwnerPlayerHash, PlayerHash);
        
        PlayerDatabase->AddRuntimeMapping(PlayerId, RuntimeId, EntityRecord.PersistentId);

        // 위치 설정 (SetPosition을 통해 셀 변경 이벤트 발생)
        FVector SpawnLocation = GetSpawnLocationForPlayer(PC);
        if (IHktMasterStashInterface* MasterStashInterface = MasterStash->GetStash())
        {
            MasterStashInterface->SetPosition(RuntimeId, SpawnLocation);
        }
        
        FHktIntentEvent SpawnEvent;
        SpawnEvent.EventId = GenerateEventId();
        SpawnEvent.SourceEntity = RuntimeId;
        SpawnEvent.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Hkt.Event.EntitySpawn"), false);
        SpawnEvent.Location = SpawnLocation;
        SpawnEvent.bIsGlobal = false;
        
        PushIntent(SpawnEvent);
        
        for (FHktIntentEvent PendingEvent : EntityRecord.PendingEvents)
        {
            // EventId 재할당 (중복 방지)
            PendingEvent.EventId = GenerateEventId();
            PushIntent(PendingEvent);
        }
        
        UE_LOG(LogTemp, Log, TEXT("HktGameMode: Loaded entity %d for player %s (Persistent: %s)"), 
            RuntimeId.RawValue, *PlayerId, *EntityRecord.PersistentId.ToString());
    }
    
    UE_LOG(LogTemp, Log, TEXT("HktGameMode: Loaded %d entities for player %s"), 
        Record.OwnedEntities.Num(), *PlayerId);
}

void AHktGameMode::SavePlayerEntities(AHktPlayerController* PC)
{
    if (!MasterStash || !PlayerDatabase || !PC)
    {
        return;
    }
    
    FString PlayerId = GetPlayerId(PC);
    FHktPlayerRecord* Record = PlayerDatabase->GetPlayerRecord(PlayerId);
    if (!Record)
    {
        return;
    }
    
    IHktStashInterface* Stash = MasterStash->GetStashInterface();
    if (!Stash)
    {
        return;
    }
    
    TArray<FHktEntityId> RuntimeIds = PlayerDatabase->GetPlayerRuntimeIds(PlayerId);
    
    for (FHktEntityId RuntimeId : RuntimeIds)
    {
        if (!Stash->IsValidEntity(RuntimeId))
        {
            continue;
        }
        
        FGuid PersistentId = PlayerDatabase->GetPersistentId(PlayerId, RuntimeId);
        FHktEntityRecord* EntityRecord = Record->FindEntityByPersistentId(PersistentId);
        
        if (!EntityRecord)
        {
            continue;
        }
        
        EntityRecord->Properties.Empty();
        for (uint16 PropId = 0; PropId < 100; ++PropId)
        {
            int32 Value = Stash->GetProperty(RuntimeId, PropId);
            if (Value != 0)
            {
                EntityRecord->SetProperty(PropId, Value);
            }
        }
        
        EntityRecord->Tags = Stash->GetTags(RuntimeId);
        EntityRecord->Tags.RemoveTag(FGameplayTag::RequestGameplayTag(TEXT("Owner.Self"), false));
        
        UE_LOG(LogTemp, Verbose, TEXT("HktGameMode: Saved entity %d (Persistent: %s)"), 
            RuntimeId.RawValue, *PersistentId.ToString());
    }
    
    PlayerDatabase->SavePlayerRecord(*Record);
    
    UE_LOG(LogTemp, Log, TEXT("HktGameMode: Saved entities for player %s"), *PlayerId);
}

FVector AHktGameMode::GetSpawnLocationForPlayer_Implementation(AHktPlayerController* PC)
{
    return FVector(0.0f, 0.0f, 100.0f);
}

IHktStashInterface* AHktGameMode::GetStashInterface() const
{
    return MasterStash ? MasterStash->GetStashInterface() : nullptr;
}

void AHktGameMode::PushIntent(const FHktIntentEvent& Event)
{
    FScopeLock Lock(&IntentLock);
    CollectedIntents.Add(Event);

    // HktInsights: CollectedIntents에 추가됨 (Queued 상태)
    HKT_INSIGHTS_UPDATE_INTENT_STATE(Event.EventId, EHktInsightsEventState::Queued);
}

void AHktGameMode::ProcessFrame()
{
    if (!GridRelevancy || !MasterStash)
    {
        return;
    }

    if (GridRelevancy)
    {
        GridRelevancy->UpdateRelevancy();
    }

    const TArray<AHktPlayerController*>& AllClients = GridRelevancy->GetAllClients();
    
    // 1. Intent 가져오기 (락 최소화)
    {
        FScopeLock Lock(&IntentLock);
        if (CollectedIntents.IsEmpty() && AllClients.IsEmpty())
        {
            return;
        }
        FrameIntents = MoveTemp(CollectedIntents);
        CollectedIntents.Reset();
    }

    // HktInsights: 배치 처리 시작 (Batched 상태)
#if WITH_HKT_INSIGHTS
    for (const FHktIntentEvent& Event : FrameIntents)
    {
        HKT_INSIGHTS_UPDATE_INTENT_STATE(Event.EventId, EHktInsightsEventState::Batched);
    }
#endif

    // 2. 이벤트별 셀 정보 미리 계산 (메인 스레드)
    ProcessFrameEventCell();

    // 3. 클라이언트별 병렬 처리
    //    - 각 클라이언트는 독립적으로 자신의 배치 생성
    //    - 읽기 전용 데이터: FrameIntents, EventCellCache, GridRelevancy
    //    - 쓰기 데이터: 각 PC의 Relevancy, 각 PC의 Batch (독립적)
    
    const int32 NumClients = AllClients.Num();
    TArray<FHktFrameBatch> Batches;
    Batches.SetNum(NumClients);

    ParallelFor(NumClients, [&](int32 ClientIndex)
    {
        AHktPlayerController* PC = AllClients[ClientIndex];
        FHktFrameBatch& Batch = Batches[ClientIndex];
        ProcessFrameClientBatch(PC, Batch);
    });

    // 4. 배치 전송 (메인 스레드 - RPC는 메인에서)
    for (int32 i = 0; i < NumClients; ++i)
    {
        if (!Batches[i].IsEmpty())
        {
            AllClients[i]->SendBatchToOwningClient(Batches[i]);
        }
    }

    // 5. 서버 VMProcessor 실행
    if (VMProcessor && VMProcessor->IsInitialized())
    {
        // HktInsights: VMProcessor에 전달 (Dispatched 상태)
#if WITH_HKT_INSIGHTS
        for (const FHktIntentEvent& Event : FrameIntents)
        {
            HKT_INSIGHTS_UPDATE_INTENT_STATE(Event.EventId, EHktInsightsEventState::Dispatched);
        }
#endif

        // 모든 이벤트를 VMProcessor에 큐잉
        VMProcessor->NotifyIntentEvents(GetFrameNumber(), FrameIntents);
    }
}

void AHktGameMode::ProcessFrameEventCell()
{
    const int32 NumEvents = FrameIntents.Num();
    EventCellCache.SetNum(NumEvents);

    for (int32 i = 0; i < NumEvents; ++i)
    {
        const FHktIntentEvent& Event = FrameIntents[i];
        FEventCellInfo& Info = EventCellCache[i];

        Info.bIsGlobal = Event.bIsGlobal;

        if (Event.bIsGlobal)
        {
            Info.bHasValidLocation = false;
        }
        else
        {
            FVector SourceLocation;
            Info.bHasValidLocation = MasterStash->TryGetPosition(Event.SourceEntity, SourceLocation);
            if (Info.bHasValidLocation)
            {
                Info.Cell = GridRelevancy->LocationToCell(SourceLocation);
            }
        }
    }
}

void AHktGameMode::ProcessFrameClientBatch(AHktPlayerController*& PC, FHktFrameBatch& Batch)
{
    Batch.FrameNumber = GetFrameNumber();

    // === 1. 이벤트 필터링 (셀 기반) ===
    const int32 NumEvents = FrameIntents.Num();
    for (int32 EventIndex = 0; EventIndex < NumEvents; ++EventIndex)
    {
        const FHktIntentEvent& Event = FrameIntents[EventIndex];
        const FEventCellInfo& Info = EventCellCache[EventIndex];

        bool bRelevant = false;

        if (Info.bIsGlobal)
        {
            bRelevant = true;
        }
        else if (Info.bHasValidLocation)
        {
            bRelevant = GridRelevancy->IsClientInterestedInCell(PC, Info.Cell);
        }
        else
        {
            // 위치 없는 이벤트는 글로벌 처리
            bRelevant = true;
        }

        if (bRelevant)
        {
            Batch.Events.Add(Event);
        }
    }

    // === 2. 셀 기반 엔티티 Relevancy (GridRelevancy에서 계산됨) ===

    // 새로 보이는 엔티티 스냅샷 추가
    TArray<FHktEntityId> NewlyVisible = GridRelevancy->GetNewlyVisibleEntities(PC);
    for (FHktEntityId EntityId : NewlyVisible)
    {
        FHktEntitySnapshot Snapshot = MasterStash->CreateEntitySnapshot(EntityId);
        if (Snapshot.IsValid())
        {
            Batch.Snapshots.Add(Snapshot);
        }
    }

    // Relevancy를 벗어난 엔티티 제거 목록 추가
    TArray<FHktEntityId> Removed = GridRelevancy->GetRemovedEntities(PC);
    for (FHktEntityId EntityId : Removed)
    {
        Batch.RemovedEntities.Add(EntityId);
    }
}