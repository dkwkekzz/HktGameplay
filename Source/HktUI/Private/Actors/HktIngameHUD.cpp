// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIngameHUD.h"
#include "HktUIElement.h"
#include "HktWorldViewAnchorStrategy.h"
#include "HktSlateView.h"
#include "DataAssets/HktWidgetEntityHudDataAsset.h"
#include "Widgets/SHktIngameHudWidget.h"
#include "Widgets/SHktEntityHudWidget.h"
#include "HktUITags.h"
#include "HktUILog.h"
#include "HktCoreEventLog.h"
#include "HktUIHelpers.h"
#include "HktAssetSubsystem.h"
#include "HktPresentationSubsystem.h"
#include "IHktPlayerInteractionInterface.h"
#include "GameFramework/PlayerController.h"

void AHktIngameHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!IngameWidgetTag.IsValid())
		IngameWidgetTag = HktGameplayTags::Widget_IngameHud;
	if (!EntityWidgetTag.IsValid())
		EntityWidgetTag = HktGameplayTags::Widget_EntityHud;

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	// Entity HUD DataAsset 비동기 로드 및 캐싱
	if (UHktAssetSubsystem* AssetSubsystem = UHktAssetSubsystem::Get(GetWorld()))
	{
		AssetSubsystem->LoadAssetAsync(EntityWidgetTag, [this](UHktTagDataAsset* Asset)
		{
			CachedEntityHudAsset = Cast<UHktWidgetEntityHudDataAsset>(Asset);
		});
	}

	LoadAndCreateWidget(IngameWidgetTag, [PC](UHktUIElement* Element)
	{
		if (Element && Element->View.IsValid())
		{
			TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
			TSharedPtr<SHktIngameHudWidget> IngameWidget = StaticCastSharedRef<SHktIngameHudWidget>(SlateWidget);
			if (IngameWidget.IsValid())
				IngameWidget->SetOwningPlayerController(PC);
		}
	});

	// PresentationSubsystem에 UI 렌더러로 등록
	if (UHktPresentationSubsystem* PresentationSubsystem = UHktPresentationSubsystem::Get(PC))
	{
		PresentationSubsystem->RegisterRenderer(this);
	}
}

void AHktIngameHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// PresentationSubsystem에서 해제
	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (UHktPresentationSubsystem* PresentationSubsystem = UHktPresentationSubsystem::Get(PC))
		{
			PresentationSubsystem->UnregisterRenderer(this);
		}
	}

	TrackedEntities.Empty();
	CachedEntityHudAsset = nullptr;
	bInitialSyncDone = false;

	Super::EndPlay(EndPlayReason);
}

// --- IHktPresentationRenderer ---

void AHktIngameHUD::Sync(const FHktPresentationState& State)
{
	SyncEntityElements(State);
	UpdateEntityProperties(State);
	UpdateAllElements();
}

void AHktIngameHUD::Teardown()
{
	TrackedEntities.Empty();
	bInitialSyncDone = false;
}

// --- Entity 동기화 ---

void AHktIngameHUD::SyncEntityElements(const FHktPresentationState& State)
{
	if (!bInitialSyncDone)
	{
		// 초기 동기화: PresentationState의 모든 유효 엔티티에 대해 위젯 생성
		State.ForEachEntity([this, &State](const FHktEntityPresentation& Entity)
		{
			if (Entity.EntityId == InvalidEntityId) return;
			TrackedEntities.Add(Entity.EntityId);
			CreateEntityElement(Entity.EntityId, State);
		});
		bInitialSyncDone = true;

		HKT_EVENT_LOG(HktLogTags::UI, EHktLogLevel::Info, EHktLogSource::Client, FString::Printf(TEXT("HUD: InitialSync via Presentation, Entities=%d"),
			TrackedEntities.Num()));
		return;
	}

	// 신규 엔티티 추가
	for (FHktEntityId Id : State.SpawnedThisFrame)
	{
		if (Id == InvalidEntityId) continue;
		if (!TrackedEntities.Contains(Id))
		{
			TrackedEntities.Add(Id);
			CreateEntityElement(Id, State);
		}
	}

	// 제거된 엔티티 정리
	for (FHktEntityId Id : State.RemovedThisFrame)
	{
		if (TrackedEntities.Contains(Id))
		{
			RemoveEntityElement(Id);
			TrackedEntities.Remove(Id);
		}
	}
}

void AHktIngameHUD::CreateEntityElement(FHktEntityId EntityId, const FHktPresentationState& State)
{
	UHktUIElement* Element = GetOrAddEntityElement(EntityId);
	if (!Element || Element->View.IsValid()) return;

	// DataAsset 팩토리를 통해 뷰 + 앵커 전략 생성 (미로드 시 직접 생성 폴백)
	TSharedPtr<IHktUIView> View;
	UHktWorldViewAnchorStrategy* Strategy = nullptr;
	if (CachedEntityHudAsset)
	{
		View = CachedEntityHudAsset->CreateView();
		Strategy = Cast<UHktWorldViewAnchorStrategy>(CachedEntityHudAsset->CreateStrategy(this));
	}

	if (!View.IsValid())
	{
		View = MakeShared<FHktSlateView>(SNew(SHktEntityHudWidget));
	}
	if (!Strategy)
	{
		Strategy = NewObject<UHktWorldViewAnchorStrategy>(this);
	}

	Strategy->SetTargetEntity(EntityId, EntityHudOffset);

	// PresentationState에서 엔티티 위치 설정
	const FHktEntityPresentation* Entity = State.Get(EntityId);
	if (Entity)
	{
		Strategy->SetWorldPosition(Entity->Location.Get());
	}

	Element->InitializeElement(View, Strategy);
	AddElementToCanvas(Element);

	// 뷰에서 위젯을 꺼내 초기 프로퍼티 설정
	TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(View->GetSlateWidget());
	if (!EntityWidget.IsValid()) return;

	EntityWidget->SetEntityId(EntityId);

	if (Entity)
	{
		float Health = Entity->Health.Get();
		float MaxHealth = Entity->MaxHealth.Get();
		int64 OwnerUid = Entity->OwnedPlayerUid.Get();
		int32 Team = Entity->Team.Get();

		EntityWidget->SetOwnerLabel(OwnerUid != 0 ? FString::Printf(TEXT("P:%lld"), OwnerUid) : TEXT("-"));
		EntityWidget->SetHealthPercent(MaxHealth > 0.f ? Health / MaxHealth : 1.f);

		static const FLinearColor TeamColors[] = {
			FLinearColor::White,
			FLinearColor(0.3f, 0.6f, 1.f),
			FLinearColor(1.f, 0.3f, 0.3f),
			FLinearColor(0.3f, 1.f, 0.3f),
			FLinearColor(1.f, 1.f, 0.3f)
		};
		EntityWidget->SetTeamColor(TeamColors[FMath::Clamp(Team, 0, 4)]);
	}
}

void AHktIngameHUD::UpdateEntityProperties(const FHktPresentationState& State)
{
	for (FHktEntityId EntityId : TrackedEntities)
	{
		const FHktEntityPresentation* Entity = State.Get(EntityId);
		if (!Entity) continue;

		// 앵커 전략에 최신 위치 반영
		UHktUIElement* Element = FindEntityElement(EntityId);
		if (!Element) continue;

		UHktWorldViewAnchorStrategy* Strategy = Cast<UHktWorldViewAnchorStrategy>(Element->AnchorStrategy);
		if (Strategy)
		{
			Strategy->SetWorldPosition(Entity->Location.Get());
		}

		// 위젯 프로퍼티 갱신
		if (!Element->View.IsValid()) continue;

		TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
		TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(SlateWidget);
		if (!EntityWidget.IsValid()) continue;

		float Health = Entity->Health.Get();
		float MaxHealth = Entity->MaxHealth.Get();
		int64 OwnerUid = Entity->OwnedPlayerUid.Get();
		int32 Team = Entity->Team.Get();

		EntityWidget->SetHealthPercent(MaxHealth > 0.f ? Health / MaxHealth : 0.f);
		EntityWidget->SetOwnerLabel(OwnerUid != 0 ? FString::Printf(TEXT("P:%lld"), OwnerUid) : TEXT("-"));

		static const FLinearColor TeamColors[] = {
			FLinearColor::White,
			FLinearColor(0.3f, 0.6f, 1.f),
			FLinearColor(1.f, 0.3f, 0.3f),
			FLinearColor(0.3f, 1.f, 0.3f),
			FLinearColor(1.f, 1.f, 0.3f)
		};
		EntityWidget->SetTeamColor(TeamColors[FMath::Clamp(Team, 0, 4)]);
	}
}
