// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIngameHUD.h"
#include "HktUIElement.h"
#include "HktWorldViewAnchorStrategy.h"
#include "HktSlateView.h"
#include "Widgets/SHktIngameHudWidget.h"
#include "Widgets/SHktEntityHudWidget.h"
#include "HktGameplayTags.h"
#include "HktPropertyIds.h"
#include "HktUIHelpers.h"
#include "IHktPlayerInteractionInterface.h"
#include "GameFramework/PlayerController.h"

void AHktIngameHUD::BeginPlay()
{
	Super::BeginPlay();

	// 뷰포트 위젯 태그 기본값
	if (!IngameWidgetTag.IsValid())
	{
		IngameWidgetTag = HktGameplayTags::Widget_IngameHud;
	}
	if (!EntityWidgetTag.IsValid())
	{
		EntityWidgetTag = HktGameplayTags::Widget_EntityHud;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	// WorldView 갱신 이벤트 구독
	if (IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(PC))
	{
		Interaction->OnWorldViewUpdated().AddUObject(this, &AHktIngameHUD::OnWorldViewUpdated);
	}

	// 뷰포트 HUD 위젯 로드
	LoadAndCreateWidget(IngameWidgetTag, [PC](UHktUIElement* Element)
	{
		if (Element && Element->View.IsValid())
		{
			TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
			TSharedPtr<SHktIngameHudWidget> IngameWidget = StaticCastSharedRef<SHktIngameHudWidget>(SlateWidget);
			if (IngameWidget.IsValid())
			{
				IngameWidget->SetOwningPlayerController(PC);
			}
		}
	});
}

void AHktIngameHUD::OnWorldViewUpdated()
{
	RefreshWorldView();
	if (!bWorldViewValid) return;

	UpdateEntityUI();

	UpdateAllElements();
}

void AHktIngameHUD::RefreshWorldView()
{
	IHktPlayerInteractionInterface* Interaction = GetPlayerInteraction();
	if (!Interaction)
	{
		bWorldViewValid = false;
		return;
	}

	bWorldViewValid = Interaction->GetWorldView(CachedWorldView);
}

void AHktIngameHUD::UpdateEntityUI()
{
	if (!bWorldViewValid || !CachedWorldView.WorldState) return;

	SyncEntityElements();
	UpdateEntityProperties();
}

void AHktIngameHUD::SyncEntityElements()
{
	if (!bInitialSyncDone)
	{
		// 초기: 전체 엔티티 순회
		CachedWorldView.ForEachEntity(PropertyId::EntityType, [this](FHktEntityId EntityId)
		{
			if (EntityId == InvalidEntityId) return;
			TrackedEntities.Add(EntityId);
			CreateEntityElement(EntityId);
		});
		bInitialSyncDone = true;
	}
	else
	{
		// 이후: 변경된 EntityType만 순회 → 신규 엔티티 감지
		CachedWorldView.ForEachDirtyEntity(PropertyId::EntityType, [this](FHktEntityId EntityId, int32 Value)
		{
			if (!TrackedEntities.Contains(EntityId))
			{
				TrackedEntities.Add(EntityId);
				CreateEntityElement(EntityId);
			}
		});

		// 삭제된 엔티티 감지 (WorldView API로 존재 여부 확인)
		for (auto It = TrackedEntities.CreateIterator(); It; ++It)
		{
			if (CachedWorldView.GetValue(*It, PropertyId::EntityType) == 0)
			{
				RemoveEntityElement(*It);
				It.RemoveCurrent();
			}
		}
	}
}

void AHktIngameHUD::CreateEntityElement(FHktEntityId EntityId)
{
	UHktUIElement* Element = GetOrAddEntityElement(EntityId);
	if (!Element || Element->View.IsValid()) return;

	// SHktEntityHudWidget 직접 생성 (비동기 로드 없이 즉시)
	TSharedRef<SHktEntityHudWidget> EntityWidget = SNew(SHktEntityHudWidget);
	TSharedPtr<IHktUIView> View = MakeShared<FHktSlateView>(EntityWidget);

	// WorldViewAnchorStrategy 생성 및 설정
	UHktWorldViewAnchorStrategy* Strategy = NewObject<UHktWorldViewAnchorStrategy>(this);
	Strategy->SetTargetEntity(EntityId, EntityHudOffset);
	Strategy->SetWorldView(&CachedWorldView);

	Element->InitializeElement(View, Strategy);
	AddElementToCanvas(Element);

	// 초기 데이터 설정
	int32 Health = CachedWorldView.GetValue(EntityId, PropertyId::Health);
	int32 MaxHealth = CachedWorldView.GetValue(EntityId, PropertyId::MaxHealth);
	int32 OwnerHash = CachedWorldView.GetValue(EntityId, PropertyId::OwnerPlayerHash);
	int32 Team = CachedWorldView.GetValue(EntityId, PropertyId::Team);

	EntityWidget->SetEntityId(EntityId);
	EntityWidget->SetOwnerLabel(OwnerHash != 0 ? FString::Printf(TEXT("P:%d"), OwnerHash) : TEXT("-"));
	EntityWidget->SetHealthPercent(MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 1.f);

	// 팀 색상
	static const FLinearColor TeamColors[] = {
		FLinearColor::White,
		FLinearColor(0.3f, 0.6f, 1.f),
		FLinearColor(1.f, 0.3f, 0.3f),
		FLinearColor(0.3f, 1.f, 0.3f),
		FLinearColor(1.f, 1.f, 0.3f)
	};
	EntityWidget->SetTeamColor(TeamColors[FMath::Clamp(Team, 0, 4)]);
}

void AHktIngameHUD::UpdateEntityProperties()
{
	// WorldViewAnchorStrategy의 WorldView 포인터 갱신
	for (FHktEntityId EntityId : TrackedEntities)
	{
		UHktUIElement* Element = FindEntityElement(EntityId);
		if (!Element) continue;

		UHktWorldViewAnchorStrategy* Strategy = Cast<UHktWorldViewAnchorStrategy>(Element->AnchorStrategy);
		if (Strategy)
		{
			Strategy->SetWorldView(&CachedWorldView);
		}
	}

	// Health 변경 반영
	CachedWorldView.ForEachDirtyEntity(PropertyId::Health, [this](FHktEntityId EntityId, int32 Value)
	{
		UHktUIElement* Element = FindEntityElement(EntityId);
		if (!Element || !Element->View.IsValid()) return;

		TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
		TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(SlateWidget);
		if (!EntityWidget.IsValid()) return;

		int32 MaxHealth = CachedWorldView.GetValue(EntityId, PropertyId::MaxHealth);
		EntityWidget->SetHealthPercent(MaxHealth > 0 ? static_cast<float>(Value) / MaxHealth : 0.f);
	});

	// OwnerPlayerHash 변경 반영
	CachedWorldView.ForEachDirtyEntity(PropertyId::OwnerPlayerHash, [this](FHktEntityId EntityId, int32 Value)
	{
		UHktUIElement* Element = FindEntityElement(EntityId);
		if (!Element || !Element->View.IsValid()) return;

		TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
		TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(SlateWidget);
		if (!EntityWidget.IsValid()) return;

		EntityWidget->SetOwnerLabel(Value != 0 ? FString::Printf(TEXT("P:%d"), Value) : TEXT("-"));
	});

	// Team 변경 반영
	CachedWorldView.ForEachDirtyEntity(PropertyId::Team, [this](FHktEntityId EntityId, int32 Value)
	{
		UHktUIElement* Element = FindEntityElement(EntityId);
		if (!Element || !Element->View.IsValid()) return;

		TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
		TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(SlateWidget);
		if (!EntityWidget.IsValid()) return;

		static const FLinearColor TeamColors[] = {
			FLinearColor::White,
			FLinearColor(0.3f, 0.6f, 1.f),
			FLinearColor(1.f, 0.3f, 0.3f),
			FLinearColor(0.3f, 1.f, 0.3f),
			FLinearColor(1.f, 1.f, 0.3f)
		};
		EntityWidget->SetTeamColor(TeamColors[FMath::Clamp(Value, 0, 4)]);
	});
}
