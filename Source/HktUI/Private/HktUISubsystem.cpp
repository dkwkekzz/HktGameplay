// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktUISubsystem.h"
#include "IHktUserEventDispatcher.h"
#include "HktEventParam.h"
#include "HktAssetSubsystem.h"
#include "DataAssets/HktUIActionDataAsset.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"

// ============================================================================
// Construction
// ============================================================================

UHktUISubsystem::UHktUISubsystem()
	: ULocalPlayerSubsystem()
	, FTickableGameObject()
{
}

UHktUISubsystem::~UHktUISubsystem()
{
}

// ============================================================================
// USubsystem Interface
// ============================================================================

void UHktUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	check(!bInitialized);
	bInitialized = true;
	SetTickableTickType(GetTickableTickType());

	UE_LOG(LogTemp, Log, TEXT("[HktUISubsystem] Initialized"));
}

void UHktUISubsystem::Deinitialize()
{
	UnbindDispatcher();

	// 모든 관리 위젯 정리
	TArray<FGameplayTag> Tags;
	ManagedWidgets.GetKeys(Tags);
	for (const FGameplayTag& Tag : Tags)
	{
		DestroyManagedWidget(Tag);
	}

	check(bInitialized);
	bInitialized = false;
	SetTickableTickType(ETickableTickType::Never);

	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("[HktUISubsystem] Deinitialized"));
}

UHktUISubsystem* UHktUISubsystem::Get(APlayerController* PC)
{
	if (PC && PC->GetLocalPlayer())
	{
		return PC->GetLocalPlayer()->GetSubsystem<UHktUISubsystem>();
	}
	return nullptr;
}

void UHktUISubsystem::BeginDestroy()
{
	Super::BeginDestroy();
	ensureMsgf(!bInitialized, TEXT("UHktUISubsystem destroyed while still initialized!"));
}

// ============================================================================
// FTickableGameObject
// ============================================================================

UWorld* UHktUISubsystem::GetTickableGameObjectWorld() const { return GetWorld(); }

ETickableTickType UHktUISubsystem::GetTickableTickType() const
{
	return (IsTemplate() || !bInitialized) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UHktUISubsystem::IsAllowedToTick() const
{
	return bInitialized;
}

void UHktUISubsystem::Tick(float DeltaTime)
{
	checkf(bInitialized, TEXT("Ticking uninitialized subsystem!"));

	TickEntityWidgets();
}

TStatId UHktUISubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHktUISubsystem, STATGROUP_Tickables);
}

// ============================================================================
// ULocalPlayerSubsystem
// ============================================================================

void UHktUISubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	if (NewPlayerController)
	{
		BindDispatcher(NewPlayerController);
	}
	else
	{
		UnbindDispatcher();
	}
}

bool UHktUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) { return false; }

	ULocalPlayer* LP = Cast<ULocalPlayer>(Outer);
	if (!LP) { return false; }

	UWorld* World = LP->GetWorld();
	return !(World && World->GetNetMode() == NM_DedicatedServer);
}

// ============================================================================
// Dispatcher 바인딩
// ============================================================================

void UHktUISubsystem::BindDispatcher(APlayerController* PC)
{
	if (bIsBound) { UnbindDispatcher(); }
	if (!PC) { return; }

	Dispatcher = Cast<IHktUserEventDispatcher>(PC);
	if (!Dispatcher)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktUISubsystem] PlayerController does not implement IHktUserEventDispatcher"));
		return;
	}

	CachedPlayerController = PC;

	EntityCreatedHandle = Dispatcher->OnEntityCreated().AddUObject(this, &UHktUISubsystem::HandleEntityCreated);
	EntityDestroyedHandle = Dispatcher->OnEntityDestroyed().AddUObject(this, &UHktUISubsystem::HandleEntityDestroyed);

	bIsBound = true;
	UE_LOG(LogTemp, Log, TEXT("[HktUISubsystem] Bound to IHktUserEventDispatcher"));
}

void UHktUISubsystem::UnbindDispatcher()
{
	if (!bIsBound || !Dispatcher) { return; }

	Dispatcher->OnEntityCreated().Remove(EntityCreatedHandle);
	Dispatcher->OnEntityDestroyed().Remove(EntityDestroyedHandle);

	Dispatcher = nullptr;
	CachedPlayerController = nullptr;
	bIsBound = false;

	UE_LOG(LogTemp, Log, TEXT("[HktUISubsystem] Unbound from IHktUserEventDispatcher"));
}

// ============================================================================
// UI 이벤트 처리
// ============================================================================

void UHktUISubsystem::HandleUIEvent(const FHktUIEvent& Event)
{
	if (!Event.EventTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktUISubsystem] HandleUIEvent: Invalid EventTag"));
		return;
	}

	UHktAssetSubsystem* AssetSub = UHktAssetSubsystem::Get(GetWorld());
	if (!AssetSub)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktUISubsystem] HktAssetSubsystem not available"));
		return;
	}

	AssetSub->LoadAssetAsync(Event.EventTag, [this, Event](UHktTagDataAsset* LoadedAsset)
	{
		UHktUIActionDataAsset* ActionAsset = Cast<UHktUIActionDataAsset>(LoadedAsset);
		if (!ActionAsset)
		{
			UE_LOG(LogTemp, Warning, TEXT("[HktUISubsystem] No UIActionDataAsset for: %s"), *Event.EventTag.ToString());
			return;
		}
		ExecuteUIAction(ActionAsset, Event);
	});
}

void UHktUISubsystem::ExecuteUIAction(UHktUIActionDataAsset* ActionAsset, const FHktUIEvent& Event)
{
	if (!ActionAsset) { return; }

	switch (ActionAsset->ActionType)
	{
	case EHktUIActionType::CreateWidget:
	{
		UClass* WC = ActionAsset->WidgetClass.LoadSynchronous();
		if (WC)
		{
			CreateManagedWidget(
				WC, Event.EventTag,
				ActionAsset->AttachTarget,
				FHktEntityId(Event.EntityId),
				ActionAsset->ParentWidgetTag,
				ActionAsset->EntityAttachOffset,
				ActionAsset->EntityDrawSize);
		}
		break;
	}
	case EHktUIActionType::DestroyWidget:
	{
		DestroyManagedWidget(ActionAsset->TargetWidgetTag);
		break;
	}
	case EHktUIActionType::DispatchEvent:
	{
		UHktEventParam* Param = NewObject<UHktEventParam>(this);
		DispatchToPlayerController(ActionAsset->DispatchEventTag, Param);
		break;
	}
	case EHktUIActionType::CreateWidgetAndDispatch:
	{
		UClass* WC = ActionAsset->WidgetClass.LoadSynchronous();
		if (WC)
		{
			CreateManagedWidget(
				WC, Event.EventTag,
				ActionAsset->AttachTarget,
				FHktEntityId(Event.EntityId),
				ActionAsset->ParentWidgetTag,
				ActionAsset->EntityAttachOffset,
				ActionAsset->EntityDrawSize);
		}
		UHktEventParam* Param = NewObject<UHktEventParam>(this);
		DispatchToPlayerController(ActionAsset->DispatchEventTag, Param);
		break;
	}
	}

	UE_LOG(LogTemp, Log, TEXT("[HktUISubsystem] Action executed: %s (Type:%d, Attach:%d)"),
		*Event.EventTag.ToString(),
		static_cast<int32>(ActionAsset->ActionType),
		static_cast<int32>(ActionAsset->AttachTarget));
}

void UHktUISubsystem::DispatchToPlayerController(const FGameplayTag& EventTag, UHktEventParam* Param)
{
	if (!Dispatcher)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktUISubsystem] No dispatcher, cannot dispatch: %s"), *EventTag.ToString());
		return;
	}
	Dispatcher->DispatchUserEvent(EventTag, Param);
}

// ============================================================================
// 위젯 관리
// ============================================================================

UUserWidget* UHktUISubsystem::CreateManagedWidget(
	TSubclassOf<UUserWidget> WidgetClass,
	const FGameplayTag& WidgetTag,
	EHktUIAttachTarget AttachTarget,
	FHktEntityId EntityId,
	FGameplayTag ParentWidgetTag,
	FVector AttachOffset,
	FVector2D DrawSize)
{
	if (!WidgetClass || !WidgetTag.IsValid()) { return nullptr; }

	// 이미 존재
	if (const FHktManagedWidgetEntry* Existing = ManagedWidgets.Find(WidgetTag))
	{
		return Existing->Widget;
	}

	FHktManagedWidgetEntry Entry;
	Entry.AttachTarget = AttachTarget;
	Entry.EntityId = EntityId;
	Entry.ParentWidgetTag = ParentWidgetTag;

	switch (AttachTarget)
	{
	case EHktUIAttachTarget::Viewport:
		Entry.Widget = AttachToViewport(WidgetClass);
		break;

	case EHktUIAttachTarget::Widget:
		Entry.Widget = AttachToWidget(WidgetClass, ParentWidgetTag);
		break;

	case EHktUIAttachTarget::Entity:
		Entry.WidgetComponent = AttachToEntity(WidgetClass, EntityId, AttachOffset, DrawSize);
		if (Entry.WidgetComponent)
		{
			Entry.Widget = Cast<UUserWidget>(Entry.WidgetComponent->GetWidget());
		}
		break;
	}

	if (Entry.IsValid())
	{
		ManagedWidgets.Add(WidgetTag, Entry);
		UE_LOG(LogTemp, Log, TEXT("[HktUISubsystem] Created widget: %s (Attach:%d)"),
			*WidgetTag.ToString(), static_cast<int32>(AttachTarget));
	}

	return Entry.Widget;
}

void UHktUISubsystem::DestroyManagedWidget(const FGameplayTag& WidgetTag)
{
	FHktManagedWidgetEntry* Entry = ManagedWidgets.Find(WidgetTag);
	if (!Entry) { return; }

	// WidgetComponent가 있으면(Entity 부착) 컴포넌트 파괴
	if (Entry->WidgetComponent)
	{
		Entry->WidgetComponent->DestroyComponent();
		Entry->WidgetComponent = nullptr;
	}

	// Viewport/Widget 부착이면 RemoveFromParent
	if (Entry->Widget && !Entry->WidgetComponent)
	{
		Entry->Widget->RemoveFromParent();
	}

	ManagedWidgets.Remove(WidgetTag);
	UE_LOG(LogTemp, Log, TEXT("[HktUISubsystem] Destroyed widget: %s"), *WidgetTag.ToString());
}

void UHktUISubsystem::DestroyEntityWidgets(FHktEntityId EntityId)
{
	TArray<FGameplayTag> ToRemove;
	for (const auto& Pair : ManagedWidgets)
	{
		if (Pair.Value.AttachTarget == EHktUIAttachTarget::Entity && Pair.Value.EntityId == EntityId)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FGameplayTag& Tag : ToRemove)
	{
		DestroyManagedWidget(Tag);
	}
}

UUserWidget* UHktUISubsystem::GetManagedWidget(const FGameplayTag& WidgetTag) const
{
	const FHktManagedWidgetEntry* Entry = ManagedWidgets.Find(WidgetTag);
	return Entry ? Entry->Widget : nullptr;
}

// ============================================================================
// 위젯 부착 구현
// ============================================================================

UUserWidget* UHktUISubsystem::AttachToViewport(TSubclassOf<UUserWidget> WidgetClass)
{
	APlayerController* PC = CachedPlayerController.Get();
	if (!PC) { return nullptr; }

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (Widget)
	{
		Widget->AddToViewport();
	}
	return Widget;
}

UUserWidget* UHktUISubsystem::AttachToWidget(TSubclassOf<UUserWidget> WidgetClass, const FGameplayTag& ParentTag)
{
	APlayerController* PC = CachedPlayerController.Get();
	if (!PC) { return nullptr; }

	// 부모 위젯 찾기
	const FHktManagedWidgetEntry* ParentEntry = ManagedWidgets.Find(ParentTag);
	if (!ParentEntry || !ParentEntry->Widget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktUISubsystem] Parent widget not found: %s, falling back to Viewport"),
			*ParentTag.ToString());
		return AttachToViewport(WidgetClass);
	}

	// 위젯 생성 후 부모의 NamedSlot이나 Overlay에 추가하는 것은
	// UMG 설계에 따라 다르므로 기본적으로 Viewport에 추가
	// 실제 자식 부착은 Blueprint에서 처리하거나 별도 인터페이스 정의
	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (Widget)
	{
		Widget->AddToViewport();
	}
	return Widget;
}

UWidgetComponent* UHktUISubsystem::AttachToEntity(
	TSubclassOf<UUserWidget> WidgetClass,
	FHktEntityId EntityId,
	const FVector& Offset,
	const FVector2D& DrawSize)
{
	UWorld* World = GetWorld();
	if (!World || !Dispatcher) { return nullptr; }

	// 엔티티 위치 조회 (Actor/MassEntity/Stash 무관)
	FHktEntityLocationInfo LocInfo = Dispatcher->GetEntityLocationInfo(EntityId);
	if (!LocInfo.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktUISubsystem] Entity %d location not valid, deferring widget attach"), EntityId);
		// 위치가 아직 없어도 WidgetComponent는 생성해두고 Tick에서 업데이트
	}

	// Screen Space WidgetComponent용 임시 Actor 생성
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Anchor = World->SpawnActor<AActor>(AActor::StaticClass(), LocInfo.WorldLocation + Offset, FRotator::ZeroRotator, SpawnParams);
	if (!Anchor) { return nullptr; }

	UWidgetComponent* WidgetComp = NewObject<UWidgetComponent>(Anchor);
	WidgetComp->SetupAttachment(Anchor->GetRootComponent());
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComp->SetWidgetClass(WidgetClass);
	WidgetComp->SetDrawSize(DrawSize);
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComp->RegisterComponent();

	return WidgetComp;
}

// ============================================================================
// Entity 위젯 Tick
// ============================================================================

void UHktUISubsystem::TickEntityWidgets()
{
	if (!Dispatcher) { return; }

	TArray<FGameplayTag> ToRemove;

	for (auto& Pair : ManagedWidgets)
	{
		FHktManagedWidgetEntry& Entry = Pair.Value;
		if (Entry.AttachTarget != EHktUIAttachTarget::Entity) { continue; }
		if (!Entry.WidgetComponent) { continue; }

		// Dispatcher에게 위치 요청
		FHktEntityLocationInfo LocInfo = Dispatcher->GetEntityLocationInfo(Entry.EntityId);

		if (!LocInfo.bIsValid)
		{
			// 엔티티 사라짐 → 위젯 제거 예약
			ToRemove.Add(Pair.Key);
			continue;
		}

		// Anchor Actor 위치 갱신
		AActor* Anchor = Entry.WidgetComponent->GetOwner();
		if (Anchor)
		{
			Anchor->SetActorLocation(LocInfo.WorldLocation + LocInfo.AttachOffset);
		}
	}

	for (const FGameplayTag& Tag : ToRemove)
	{
		DestroyManagedWidget(Tag);
	}
}

// ============================================================================
// 엔티티 이벤트
// ============================================================================

void UHktUISubsystem::HandleEntityCreated(FHktEntityId EntityId)
{
	// 엔티티 생성 자체로 자동 HUD를 붙이지 않음.
	// DataAsset 기반으로 필요한 위젯을 붙이는 구조.
	// 외부에서 HandleUIEvent(Entity용 태그)를 호출하거나,
	// PlayerController가 DispatchUserEvent + DataAsset으로 처리.
	UE_LOG(LogTemp, Verbose, TEXT("[HktUISubsystem] Entity created: %d"), EntityId);
}

void UHktUISubsystem::HandleEntityDestroyed(FHktEntityId EntityId)
{
	// 해당 EntityId에 부착된 모든 위젯 제거
	DestroyEntityWidgets(EntityId);
	UE_LOG(LogTemp, Verbose, TEXT("[HktUISubsystem] Entity destroyed, widgets cleaned: %d"), EntityId);
}
