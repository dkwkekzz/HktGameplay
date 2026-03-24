// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIngamePlayerController.h"
#include "HktRuntimeLog.h"
#include "HktPlayerState.h"
#include "HktClientRuleInterfaces.h"
#include "HktGameMode.h"
#include "HktRuntimeConverter.h"
#include "HktRuntimeTypes.h"
#include "HktBagComponent.h"
#include "HktCoreDataCollector.h"
#include "HktCoreEventLog.h"
#include "HktStoryBuilder.h"
#include "HktRuntimeTags.h"
#include "HktCoreProperties.h"
#include "HktCoreEvents.h"
#include "HktWorldView.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagsManager.h"
#include "HktRuntimeTags.h"

AHktIngamePlayerController::AHktIngamePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void AHktIngamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }

    // ClientRule — Standalone / Client 에서만 (DedicatedServer 제외)
    const ENetMode NetMode = GetWorld()->GetNetMode();
    if (NetMode == NM_Standalone || NetMode == NM_Client)
    {
        CachedClientRule = HktRule::GetClientRule(GetWorld());
    }

    // 컴포넌트에서 인터페이스 캐싱
    TArray<UActorComponent*> Components;
    GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (IHktIntentBuilder* IntentBuilder = Cast<IHktIntentBuilder>(Comp))
        {
            CachedIntentBuilder = IntentBuilder;
        }
        else if (IHktUnitSelectionPolicy* SelectionPolicy = Cast<IHktUnitSelectionPolicy>(Comp))
        {
            CachedSelectionPolicy = SelectionPolicy;
        }
        else if (IHktProxySimulator* ProxySimulator = Cast<IHktProxySimulator>(Comp))
        {
            CachedProxySimulator = ProxySimulator;
        }
        else if (IHktCommandContainer* CommandContainer = Cast<IHktCommandContainer>(Comp))
        {
            CachedCommandContainer = CommandContainer;
            CommandContainer->InitializeSlots(SlotInputActions.Num());
        }
        else if (IHktWorldPlayer* WorldPlayer = Cast<IHktWorldPlayer>(Comp))
        {
            CachedWorldPlayer = WorldPlayer;
        }

        if (UHktBagComponent* BagComp = Cast<UHktBagComponent>(Comp))
        {
            CachedBagComponent = BagComp;
        }
    }

    // 컨텍스트 바인딩 — ServerRule::BindContext와 동일한 패턴
    if (CachedClientRule)
    {
        CachedClientRule->BindContext(
            CachedProxySimulator,
            CachedIntentBuilder,
            CachedSelectionPolicy,
            CachedCommandContainer,
            CachedWorldPlayer);
    }

    // ProxySimulatorComponent가 먼저 틱한 후 PlayerController Tick이 실행되도록 보장
    if (UActorComponent* ProxyComp = Cast<UActorComponent>(CachedProxySimulator))
    {
        AddTickPrerequisiteComponent(ProxyComp);
    }
}

void AHktIngamePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CachedClientRule)
    {
        CachedClientRule->BindContext(nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    CachedClientRule       = nullptr;
    CachedIntentBuilder    = nullptr;
    CachedSelectionPolicy  = nullptr;
    CachedProxySimulator   = nullptr;
    CachedCommandContainer = nullptr;
    CachedWorldPlayer      = nullptr;
    CachedBagComponent     = nullptr;

    Super::EndPlay(EndPlayReason);
}

void AHktIngamePlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (CachedWorldPlayer)
    {
        CachedWorldPlayer->InvalidatePlayerUidCache();
    }

    UE_LOG(LogHktRuntime, Log, TEXT("OnRep_PlayerState"));
}

void AHktIngamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput) return;

    if (SubjectAction) EnhancedInput->BindAction(SubjectAction, ETriggerEvent::Started, this, &AHktIngamePlayerController::OnSubjectAction);
    if (TargetAction)  EnhancedInput->BindAction(TargetAction,  ETriggerEvent::Started, this, &AHktIngamePlayerController::OnTargetAction);
    if (ZoomAction)    EnhancedInput->BindAction(ZoomAction,    ETriggerEvent::Triggered, this, &AHktIngamePlayerController::OnZoom);

    for (int32 i = 0; i < SlotInputActions.Num(); ++i)
    {
        if (SlotInputActions[i]) EnhancedInput->BindAction(SlotInputActions[i], ETriggerEvent::Started, this, &AHktIngamePlayerController::OnSlotAction, i);
    }
}

// ============================================================================
// 입력 이벤트 — 판단 로직은 Rule에 위임, PC는 전달만
// ============================================================================

void AHktIngamePlayerController::OnSubjectAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    Rule->OnUserEvent_SubjectInputAction();

    if (CachedIntentBuilder)
    {
        // 빈 공간 클릭 시 (Subject 미선택) → 기본 Subject로 복원
        if (CachedIntentBuilder->GetSubjectEntityId() == InvalidEntityId && DefaultSubjectEntityId != InvalidEntityId)
        {
            CachedIntentBuilder->SetSubject(DefaultSubjectEntityId);
        }

        SubjectChangedDelegate.Broadcast(CachedIntentBuilder->GetSubjectEntityId());
        HKT_EVENT_LOG_ENTITY(HktLogTags::Runtime_Intent,
            FString::Printf(TEXT("OnSubjectAction SubjectEntityId=%d"), CachedIntentBuilder->GetSubjectEntityId()),
            CachedIntentBuilder->GetSubjectEntityId());
    }
}

void AHktIngamePlayerController::OnTargetAction(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    Rule->OnUserEvent_TargetInputAction();

    if (CachedIntentBuilder)
    {
        TargetChangedDelegate.Broadcast(CachedIntentBuilder->GetTargetEntityId());

        // Rule이 빌드한 이벤트가 있으면 전송
        if (CachedIntentBuilder->HasPendingRuntimeEvent())
        {
            FHktEvent Event = CachedIntentBuilder->ConsumePendingRuntimeEvent();
            Event.PlayerUid = GetPlayerUid();
            Server_ReceiveRuntimeEvent(FHktRuntimeEvent(Event));
            IntentSubmittedDelegate.Broadcast(FHktRuntimeEvent(Event));

            HKT_EVENT_LOG_TAG(HktLogTags::Runtime_Intent,
                FString::Printf(TEXT("OnTargetAction Submit %s"), *Event.ToString()),
                Event.SourceEntity, Event.EventTag);
        }
    }
}

void AHktIngamePlayerController::OnSlotAction(const FInputActionValue& Value, int32 SlotIndex)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    Rule->OnUserEvent_CommandInputAction(SlotIndex);

    if (CachedIntentBuilder && CachedCommandContainer)
    {
        // 타겟 필요 여부 확인
        bool bTargetRequired = CachedCommandContainer->IsTargetRequiredAtSlot(SlotIndex);
        if (bTargetRequired)
        {
            // 타겟 대기 상태 — CommandChanged 브로드캐스트
            CommandChangedDelegate.Broadcast(CachedCommandContainer->GetEventTagAtSlot(SlotIndex));
            UE_LOG(LogHktRuntime, Verbose, TEXT("OnSlotAction WaitTarget Slot=%d"), SlotIndex);
            return;
        }

        // 타겟 불필요한 스킬 → Rule이 이벤트를 빌드했으면 즉시 전송
        if (CachedIntentBuilder->HasPendingRuntimeEvent())
        {
            FHktEvent Event = CachedIntentBuilder->ConsumePendingRuntimeEvent();
            Event.PlayerUid = GetPlayerUid();
            Server_ReceiveRuntimeEvent(FHktRuntimeEvent(Event));
            IntentSubmittedDelegate.Broadcast(FHktRuntimeEvent(Event));

            HKT_EVENT_LOG_TAG(HktLogTags::Runtime_Intent,
                FString::Printf(TEXT("OnSlotAction Submit Slot=%d %s"), SlotIndex, *Event.ToString()),
                Event.SourceEntity, Event.EventTag);
        }
        else
        {
            // 커맨드 대기 상태 (타겟 지정 필요)
            CommandChangedDelegate.Broadcast(CachedCommandContainer->GetEventTagAtSlot(SlotIndex));
        }
    }
}

void AHktIngamePlayerController::OnZoom(const FInputActionValue& Value)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

    if (Value.GetValueType() == EInputActionValueType::Axis1D)
    {
        float Delta = Value.Get<float>();
        Rule->OnUserEvent_ZoomInputAction(Delta);
        WheelInputDelegate.Broadcast(Delta);
    }
}

// ============================================================================
// S2C RPC
// ============================================================================

void AHktIngamePlayerController::Client_ReceiveInitialState_Implementation(const FHktRuntimeSimulationState& State, int32 GroupIndex)
{
#if ENABLE_HKT_INSIGHTS
    InsightReceivedInitialStateCount++;
#endif

    HKT_EVENT_LOG(HktLogTags::Runtime_Client, FString::Printf(TEXT("ReceiveInitialState GroupIndex=%d"), GroupIndex));
    bIsInitialSync = false;

    IHktClientRule* Rule = GetClientRule();
    if (Rule)
    {
        Rule->OnReceived_InitialState(HktRuntimeConverter::ConvertToWorldState(State), GroupIndex);
    }
}

void AHktIngamePlayerController::Client_ReceiveFrameBatch_Implementation(const FHktRuntimeBatch& Batch)
{
    IHktClientRule* Rule = GetClientRule();
    if (!Rule) return;

#if ENABLE_HKT_INSIGHTS
    InsightReceivedBatchCount++;
#endif

    HKT_EVENT_LOG(HktLogTags::Runtime_Client,
        FString::Printf(TEXT("ReceiveFrameBatch Frame=%lld Events=%d"),
            Batch.Value.FrameNumber, Batch.Value.NewEvents.Num()));
    Rule->OnReceived_FrameBatch(static_cast<const FHktSimulationEvent&>(Batch));
}

// ============================================================================
// Tick — ProxySimulator가 생성한 Diff를 소비하여 Presentation에 전달
// ============================================================================

void AHktIngamePlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!CachedProxySimulator || !CachedProxySimulator->IsInitialized()) return;

    if (!bIsInitialSync)
    {
        bIsInitialSync = true;

        // InitialState 수신 후 나의 엔티티를 기본 Subject로 설정
        ResolveDefaultSubject();

        if (CachedProxySimulator)
        {
            FHktWorldView View;
            View.WorldState = &CachedProxySimulator->GetWorldState();
            View.FrameNumber = CachedProxySimulator->GetWorldState().FrameNumber;
            View.bIsInitialSync = true;
            WorldViewUpdatedDelegate.Broadcast(View);

            // InitialSync 시 전체 슬롯 바인딩 동기화
            SyncSlotBindingsFromWorldState(View);
        }
    }

    // PlayerUid가 지연 복제되어 ResolveDefaultSubject가 실패한 경우 재시도
    if (DefaultSubjectEntityId == InvalidEntityId && CachedProxySimulator && CachedProxySimulator->IsInitialized())
    {
        ResolveDefaultSubject();
    }

    FHktSimulationDiff Diff;
    if (CachedProxySimulator->ConsumePendingDiff(Diff))
    {
        FHktWorldView View;
        View.WorldState = &CachedProxySimulator->GetWorldState();
        View.FrameNumber = Diff.FrameNumber;
        View.bIsInitialSync = false;
        View.SpawnedEntities = &Diff.SpawnedEntities;
        View.RemovedEntities = &Diff.RemovedEntities;
        View.PropertyDeltas = &Diff.PropertyDeltas;
        View.TagDeltas = &Diff.TagDeltas;
        View.OwnerDeltas = &Diff.OwnerDeltas;
        WorldViewUpdatedDelegate.Broadcast(View);

        // ActionSlot 변경이 포함된 경우 슬롯 바인딩 동기화
        SyncSlotBindingsFromWorldState(View);
    }

#if ENABLE_HKT_INSIGHTS
    // 클라이언트 런타임 상태 수집
    {
        const FString Cat = TEXT("Runtime.Client");
        FString NetModeStr;
        switch (GetWorld()->GetNetMode())
        {
        case NM_Standalone:       NetModeStr = TEXT("Standalone"); break;
        case NM_DedicatedServer:  NetModeStr = TEXT("DedicatedServer"); break;
        case NM_ListenServer:     NetModeStr = TEXT("ListenServer"); break;
        default:                  NetModeStr = TEXT("Client"); break;
        }
        HKT_INSIGHT_COLLECT(Cat, TEXT("NetMode"), NetModeStr);
        HKT_INSIGHT_COLLECT(Cat, TEXT("Role"), HasAuthority() ? TEXT("Server") : TEXT("Client"));

        if (CachedIntentBuilder)
        {
            HKT_INSIGHT_COLLECT(Cat, TEXT("Subject"), FString::FromInt(CachedIntentBuilder->GetSubjectEntityId()));
            FGameplayTag Tag = CachedIntentBuilder->GetEventTag();
            HKT_INSIGHT_COLLECT(Cat, TEXT("Command"), Tag.IsValid() ? Tag.ToString() : TEXT("(none)"));
        }

        if (CachedProxySimulator && CachedProxySimulator->IsInitialized())
        {
            const FHktWorldState& WS = CachedProxySimulator->GetWorldState();
            HKT_INSIGHT_COLLECT(Cat, TEXT("ProxyFrame"), FString::Printf(TEXT("%lld"), WS.FrameNumber));
            HKT_INSIGHT_COLLECT(Cat, TEXT("ProxyEntities"), FString::FromInt(WS.GetEntityCount()));
        }

        if (CachedWorldPlayer)
        {
            HKT_INSIGHT_COLLECT(Cat, TEXT("PlayerUid"), FString::Printf(TEXT("%lld"), CachedWorldPlayer->GetPlayerUid()));
        }

        HKT_INSIGHT_COLLECT(Cat, TEXT("SentIntents"), FString::FromInt(InsightSentIntentCount));
        HKT_INSIGHT_COLLECT(Cat, TEXT("ReceivedBatches"), FString::FromInt(InsightReceivedBatchCount));
        HKT_INSIGHT_COLLECT(Cat, TEXT("ReceivedInitialStates"), FString::FromInt(InsightReceivedInitialStateCount));
    }
#endif
}

// ============================================================================
// C2S RPC — 통합 RuntimeEvent
// ============================================================================

bool AHktIngamePlayerController::Server_ReceiveRuntimeEvent_Validate(const FHktRuntimeEvent& Event)
{
    return Event.Value.SourceEntity != InvalidEntityId && Event.Value.EventTag.IsValid();
}

void AHktIngamePlayerController::Server_ReceiveRuntimeEvent_Implementation(const FHktRuntimeEvent& Event)
{
#if ENABLE_HKT_INSIGHTS
    InsightSentIntentCount++;
#endif

    if (AHktGameMode* GM = GetWorld()->GetAuthGameMode<AHktGameMode>())
    {
        GM->PushRuntimeEvent(GetPlayerUid(), Event.Value);
    }
}

// ============================================================================
// 아이템 상호작용 (UI 전용 — RuntimeEvent로 통합)
// ============================================================================

void AHktIngamePlayerController::RequestItemDrop(FHktEntityId ItemEntity)
{
    if (DefaultSubjectEntityId == InvalidEntityId || ItemEntity == InvalidEntityId) return;

    FHktEvent Event;
    Event.EventTag = HktGameplayTags::Story_Event_Item_Drop;
    Event.SourceEntity = DefaultSubjectEntityId;
    Event.TargetEntity = ItemEntity;
    Event.PlayerUid = GetPlayerUid();
    Server_ReceiveRuntimeEvent(FHktRuntimeEvent(Event));

    HKT_EVENT_LOG_ENTITY(HktLogTags::Runtime_Intent,
        FString::Printf(TEXT("RequestItemDrop Item=%d"), ItemEntity), DefaultSubjectEntityId);
}

void AHktIngamePlayerController::ResolveDefaultSubject()
{
    if (!CachedProxySimulator || !CachedProxySimulator->IsInitialized()) return;

    const int64 PlayerUid = GetPlayerUid();
    if (PlayerUid == 0) return;

    const FHktWorldState& WS = CachedProxySimulator->GetWorldState();
    DefaultSubjectEntityId = InvalidEntityId;

    WS.ForEachEntityByOwner(PlayerUid, [this](FHktEntityId Id, int32 /*Slot*/)
    {
        if (DefaultSubjectEntityId == InvalidEntityId)
        {
            DefaultSubjectEntityId = Id;
        }
    });

    if (DefaultSubjectEntityId != InvalidEntityId && CachedIntentBuilder)
    {
        CachedIntentBuilder->SetSubject(DefaultSubjectEntityId);
        SubjectChangedDelegate.Broadcast(DefaultSubjectEntityId);
        UE_LOG(LogHktRuntime, Log, TEXT("ResolveDefaultSubject: DefaultSubjectEntityId=%d PlayerUid=%lld"), DefaultSubjectEntityId, PlayerUid);
    }
}

/** ItemSlot0~8에 대응하는 PropertyId 테이블 */
static constexpr uint16 ItemSlotPropertyIds[] =
{
    PropertyId::ItemSlot0, PropertyId::ItemSlot1, PropertyId::ItemSlot2,
    PropertyId::ItemSlot3, PropertyId::ItemSlot4, PropertyId::ItemSlot5,
    PropertyId::ItemSlot6, PropertyId::ItemSlot7, PropertyId::ItemSlot8,
};
static constexpr int32 MaxItemSlots = UE_ARRAY_COUNT(ItemSlotPropertyIds);

void AHktIngamePlayerController::SyncSlotBindingsFromWorldState(const FHktWorldView& View)
{
    if (!CachedCommandContainer || !View.WorldState) return;
    if (DefaultSubjectEntityId == InvalidEntityId) return;

    const FHktWorldState& WS = *View.WorldState;

    // InitialSync 또는 ItemSlot/ItemSkillTag 프로퍼티 변경 시 동기화
    bool bNeedsSync = View.bIsInitialSync;
    if (!bNeedsSync && View.PropertyDeltas)
    {
        for (const FHktPropertyDelta& D : *View.PropertyDeltas)
        {
            if (D.PropertyId >= PropertyId::ItemSlot0 && D.PropertyId <= PropertyId::ItemSlot8)
            {
                bNeedsSync = true;
                break;
            }
            if (D.PropertyId == PropertyId::ItemSkillTag || D.PropertyId == PropertyId::ItemState)
            {
                bNeedsSync = true;
                break;
            }
        }
    }
    if (!bNeedsSync) return;

    // 모든 슬롯 클리어
    const int32 NumSlots = FMath::Min(CachedCommandContainer->GetNumSlots(), MaxItemSlots);
    for (int32 i = 0; i < NumSlots; ++i)
    {
        CachedCommandContainer->ClearSlotBinding(i);
    }

    // 캐릭터 엔티티의 ItemSlot0~8에서 아이템 EntityId 읽기 → 스킬 바인딩
    for (int32 i = 0; i < NumSlots; ++i)
    {
        const FHktEntityId ItemId = WS.GetProperty(DefaultSubjectEntityId, ItemSlotPropertyIds[i]);
        if (ItemId == 0 || !WS.IsValidEntity(ItemId))
            continue;

        // ItemSkillTag (NetIndex → FGameplayTag)
        int32 SkillTagNetIndex = WS.GetProperty(ItemId, PropertyId::ItemSkillTag);
        if (SkillTagNetIndex <= 0)
            continue;

        FName TagName = UGameplayTagsManager::Get().GetTagNameFromNetIndex(static_cast<FGameplayTagNetIndex>(SkillTagNetIndex));
        if (TagName.IsNone())
            continue;

        FGameplayTag SkillTag = FGameplayTag::RequestGameplayTag(TagName, false);
        if (!SkillTag.IsValid())
            continue;

        // 아이템의 SkillTargetRequired 프로퍼티로 타겟 필요 여부 결정 (기본값: 필요)
        bool bTargetRequired = WS.GetProperty(ItemId, PropertyId::SkillTargetRequired) != 0;
        CachedCommandContainer->SetSlotBinding(i, SkillTag, bTargetRequired);
        UE_LOG(LogHktRuntime, Log, TEXT("SyncSlotBindings: Slot %d -> %s (Item %d)"),
            i, *SkillTag.ToString(), ItemId);
    }

    // 배치 완료 후 한 번만 broadcast — UI 갱신 트리거
    SlotBindingChangedDelegate.Broadcast(-1);
}

// ============================================================================
// C2S Bag RPC
// ============================================================================

bool AHktIngamePlayerController::Server_ReceiveBagRequest_Validate(const FHktRuntimeBagRequest& Request)
{
    return Request.Value.SourceEntity != InvalidEntityId;
}

void AHktIngamePlayerController::Server_ReceiveBagRequest_Implementation(const FHktRuntimeBagRequest& Request)
{
#if ENABLE_HKT_INSIGHTS
    InsightSentIntentCount++;
#endif

    if (AHktGameMode* GM = GetWorld()->GetAuthGameMode<AHktGameMode>())
    {
        GM->PushBagRequest(GetPlayerUid(), Request.Value);
    }
}

// ============================================================================
// 가방 요청 API (UI에서 호출)
// ============================================================================

void AHktIngamePlayerController::RequestBagStore(int32 ActionSlot)
{
    if (DefaultSubjectEntityId == InvalidEntityId) return;

    FHktBagRequest Req;
    Req.Action = EHktBagAction::StoreFromSlot;
    Req.SourceEntity = DefaultSubjectEntityId;
    Req.ActionSlot = ActionSlot;
    Server_ReceiveBagRequest(FHktRuntimeBagRequest(Req));

    HKT_EVENT_LOG_ENTITY(HktLogTags::Runtime_Intent,
        FString::Printf(TEXT("RequestBagStore ActionSlot=%d"), ActionSlot), DefaultSubjectEntityId);
}

void AHktIngamePlayerController::RequestBagRestore(int32 BagSlot, int32 ActionSlot)
{
    if (DefaultSubjectEntityId == InvalidEntityId) return;

    FHktBagRequest Req;
    Req.Action = EHktBagAction::RestoreToSlot;
    Req.SourceEntity = DefaultSubjectEntityId;
    Req.BagSlot = BagSlot;
    Req.ActionSlot = ActionSlot;
    Server_ReceiveBagRequest(FHktRuntimeBagRequest(Req));

    HKT_EVENT_LOG_ENTITY(HktLogTags::Runtime_Intent,
        FString::Printf(TEXT("RequestBagRestore BagSlot=%d ActionSlot=%d"), BagSlot, ActionSlot), DefaultSubjectEntityId);
}

void AHktIngamePlayerController::RequestBagDiscard(int32 BagSlot)
{
    if (DefaultSubjectEntityId == InvalidEntityId) return;

    FHktBagRequest Req;
    Req.Action = EHktBagAction::Discard;
    Req.SourceEntity = DefaultSubjectEntityId;
    Req.BagSlot = BagSlot;
    Server_ReceiveBagRequest(FHktRuntimeBagRequest(Req));

    HKT_EVENT_LOG_ENTITY(HktLogTags::Runtime_Intent,
        FString::Printf(TEXT("RequestBagDiscard BagSlot=%d"), BagSlot), DefaultSubjectEntityId);
}

const FHktBagState* AHktIngamePlayerController::GetBagState() const
{
    return CachedBagComponent ? &CachedBagComponent->GetLocalBagState() : nullptr;
}

FOnHktBagChanged& AHktIngamePlayerController::OnBagChanged()
{
    if (CachedBagComponent)
    {
        return CachedBagComponent->OnBagChanged();
    }
    static FOnHktBagChanged Dummy;
    return Dummy;
}

IHktClientRule* AHktIngamePlayerController::GetClientRule() const
{
    return CachedClientRule;
}

// ============================================================================
// IHktPlayerInteractionInterface 구현
// ============================================================================

void AHktIngamePlayerController::ExecuteCommand(UObject* CommandData)
{
}

bool AHktIngamePlayerController::GetWorldState(const FHktWorldState*& OutState) const
{
    if (CachedProxySimulator && CachedProxySimulator->IsInitialized())
    {
        OutState = &CachedProxySimulator->GetWorldState();
        return true;
    }
    OutState = nullptr;
    return false;
}

int64 AHktIngamePlayerController::GetPlayerUid() const
{
    return CachedWorldPlayer ? CachedWorldPlayer->GetPlayerUid() : 0;
}
