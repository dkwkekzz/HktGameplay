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

void AHktIngameHUD::OnCameraViewChanged(const FHktPresentationState& State)
{
	// 카메라만 변경된 경우: 엔티티 생성/제거/프로퍼티 갱신 없이
	// 스크린 좌표 재투영만 수행 (1프레임 튀기 방지)
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
		// 아이템이 장착된 상태(IsItemAttached)인 엔티티는 월드에 배치된 것이 아니므로 제외
		State.ForEachEntity([this, &State](const FHktEntityPresentation& Entity)
		{
			if (Entity.EntityId == InvalidEntityId) return;
			if (Entity.IsItemAttached()) return;
			TrackedEntities.Add(Entity.EntityId);
			CreateEntityElement(Entity.EntityId, State);
		});
		bInitialSyncDone = true;

		HKT_EVENT_LOG(HktLogTags::UI, EHktLogLevel::Info, EHktLogSource::Client, FString::Printf(TEXT("HUD: InitialSync via Presentation, Entities=%d"),
			TrackedEntities.Num()));
		return;
	}

	// 신규 엔티티 추가 (장착 아이템은 제외)
	for (FHktEntityId Id : State.SpawnedThisFrame)
	{
		if (Id == InvalidEntityId) continue;
		const FHktEntityPresentation* Entity = State.Get(Id);
		if (Entity && Entity->IsItemAttached()) continue;
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

	// 아이템 상태 변화: 월드에 있다가 장착되면 HUD 제거, 장착 해제되면 HUD 생성
	for (FHktEntityId Id : State.DirtyThisFrame)
	{
		const FHktEntityPresentation* Entity = State.Get(Id);
		if (!Entity) continue;

		const bool bTracked = TrackedEntities.Contains(Id);
		const bool bShouldShow = !Entity->IsItemAttached();

		if (bTracked && !bShouldShow)
		{
			// 장착됨 → HUD 제거
			RemoveEntityElement(Id);
			TrackedEntities.Remove(Id);
		}
		else if (!bTracked && bShouldShow && Entity->IsAlive())
		{
			// 장착 해제됨 → HUD 생성
			TrackedEntities.Add(Id);
			CreateEntityElement(Id, State);
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

	Strategy->SetTargetEntity(EntityId, EntityHudHeadClearance);

	// PresentationState에서 엔티티 RenderLocation + CapsuleHalfHeight로 머리 위치 설정
	const FHktEntityPresentation* Entity = State.Get(EntityId);
	if (Entity)
	{
		Strategy->SetWorldPosition(Entity->RenderLocation.Get(), Entity->CapsuleHalfHeight);
	}

	Element->InitializeElement(View, Strategy);
	AddElementToCanvas(Element);

	// 뷰에서 위젯을 꺼내 초기 프로퍼티 설정
	TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(View->GetSlateWidget());
	if (!EntityWidget.IsValid()) return;

	EntityWidget->SetEntityId(EntityId);

	if (Entity)
	{
		EntityWidget->SetOwnerLabel(Entity->OwnerLabel.Get());
		EntityWidget->SetHealthPercent(Entity->HealthRatio.Get());
		EntityWidget->SetTeamColor(Entity->TeamColor.Get());
	}
}

void AHktIngameHUD::UpdateEntityProperties(const FHktPresentationState& State)
{
	const int64 Frame = State.GetCurrentFrame();

	// 장착 아이템 네임플레이트: OwnerEntity별로 장착된 아이템의 VisualElement 태그 수집
	TMap<FHktEntityId, FString> ItemLabelMap;
	State.ForEachEntity([&ItemLabelMap](const FHktEntityPresentation& ItemEntity)
	{
		if (!ItemEntity.IsItemAttached()) return;
		const FHktEntityId OwnerId = ItemEntity.OwnerEntity.Get();
		const FGameplayTag& VisTag = ItemEntity.VisualElement.Get();
		if (!VisTag.IsValid()) return;

		// 태그의 마지막 노드를 표시명으로 사용 (예: "Item.Weapon.Sword" → "Sword")
		const FString TagStr = VisTag.ToString();
		FString DisplayName;
		TagStr.Split(TEXT("."), nullptr, &DisplayName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (DisplayName.IsEmpty()) DisplayName = TagStr;

		FString& Existing = ItemLabelMap.FindOrAdd(OwnerId);
		if (!Existing.IsEmpty()) Existing += TEXT(", ");
		Existing += DisplayName;
	});

	for (FHktEntityId EntityId : TrackedEntities)
	{
		const FHktEntityPresentation* Entity = State.Get(EntityId);
		if (!Entity) continue;

		// 앵커 전략에 최신 위치 반영
		UHktUIElement* Element = FindEntityElement(EntityId);
		if (!Element) continue;

		UHktWorldViewAnchorStrategy* Strategy = Cast<UHktWorldViewAnchorStrategy>(Element->AnchorStrategy);
		if (Strategy && Entity->RenderLocation.IsDirty(Frame))
		{
			Strategy->SetWorldPosition(Entity->RenderLocation.Get(), Entity->CapsuleHalfHeight);
		}

		// 위젯 프로퍼티 갱신 (dirty check)
		if (!Element->View.IsValid()) continue;

		TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
		TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(SlateWidget);
		if (!EntityWidget.IsValid()) continue;

		if (Entity->HealthRatio.IsDirty(Frame))
			EntityWidget->SetHealthPercent(Entity->HealthRatio.Get());
		if (Entity->OwnerLabel.IsDirty(Frame))
			EntityWidget->SetOwnerLabel(Entity->OwnerLabel.Get());
		if (Entity->TeamColor.IsDirty(Frame))
			EntityWidget->SetTeamColor(Entity->TeamColor.Get());

		// 장착 아이템 네임플레이트 갱신
		const FString* ItemLabel = ItemLabelMap.Find(EntityId);
		EntityWidget->SetItemLabel(ItemLabel ? *ItemLabel : FString());
	}
}
