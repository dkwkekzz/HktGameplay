// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIngameHUD.h"
#include "HktUIElement.h"
#include "HktWorldViewAnchorStrategy.h"
#include "HktSlateView.h"
#include "Widgets/SHktIngameHudWidget.h"
#include "Widgets/SHktEntityHudWidget.h"
#include "HktUITags.h"
#include "HktCoreProperties.h"
#include "HktUIHelpers.h"
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

	if (IHktPlayerInteractionInterface* Interaction = Cast<IHktPlayerInteractionInterface>(PC))
		Interaction->OnWorldViewUpdated().AddUObject(this, &AHktIngameHUD::OnWorldViewUpdated);

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
}

void AHktIngameHUD::OnWorldViewUpdated(const FHktWorldView& View)
{
	RefreshWorldState();
	if (!bWorldStateValid) return;

	UpdateEntityUI();
	UpdateAllElements();
}

void AHktIngameHUD::RefreshWorldState()
{
	IHktPlayerInteractionInterface* Interaction = GetPlayerInteraction();
	if (!Interaction)
	{
		bWorldStateValid = false;
		CachedWorldState = nullptr;
		return;
	}

	const FHktWorldState* OutState = nullptr;
	bWorldStateValid = Interaction->GetWorldState(OutState);
	CachedWorldState = OutState;
}

void AHktIngameHUD::UpdateEntityUI()
{
	if (!bWorldStateValid || !CachedWorldState) return;

	SyncEntityElements();
	UpdateEntityProperties();
}

void AHktIngameHUD::SyncEntityElements()
{
	if (!CachedWorldState) return;

	if (!bInitialSyncDone)
	{
		CachedWorldState->ForEachEntity([this](FHktEntityId EntityId, int32 /*Slot*/)
		{
			if (EntityId == InvalidEntityId) return;
			TrackedEntities.Add(EntityId);
			CreateEntityElement(EntityId);
		});
		bInitialSyncDone = true;
	}
	else
	{
		CachedWorldState->ForEachEntity([this](FHktEntityId EntityId, int32 /*Slot*/)
		{
			if (EntityId == InvalidEntityId) return;
			if (!TrackedEntities.Contains(EntityId))
			{
				TrackedEntities.Add(EntityId);
				CreateEntityElement(EntityId);
			}
		});

		for (auto It = TrackedEntities.CreateIterator(); It; ++It)
		{
			if (!CachedWorldState->IsValidEntity(*It))
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

	TSharedRef<SHktEntityHudWidget> EntityWidget = SNew(SHktEntityHudWidget);
	TSharedPtr<IHktUIView> View = MakeShared<FHktSlateView>(EntityWidget);

	UHktWorldViewAnchorStrategy* Strategy = NewObject<UHktWorldViewAnchorStrategy>(this);
	Strategy->SetTargetEntity(EntityId, EntityHudOffset);
	Strategy->SetWorldState(CachedWorldState);

	Element->InitializeElement(View, Strategy);
	AddElementToCanvas(Element);

	int32 Health = CachedWorldState->GetProperty(EntityId, PropertyId::Health);
	int32 MaxHealth = CachedWorldState->GetProperty(EntityId, PropertyId::MaxHealth);
	int32 OwnerUid = CachedWorldState->GetProperty(EntityId, PropertyId::OwnedPlayerUid);
	int32 Team = CachedWorldState->GetProperty(EntityId, PropertyId::Team);

	EntityWidget->SetEntityId(EntityId);
	EntityWidget->SetOwnerLabel(OwnerUid != 0 ? FString::Printf(TEXT("P:%d"), OwnerUid) : TEXT("-"));
	EntityWidget->SetHealthPercent(MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 1.f);

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
	if (!CachedWorldState) return;

	for (FHktEntityId EntityId : TrackedEntities)
	{
		UHktUIElement* Element = FindEntityElement(EntityId);
		if (!Element) continue;

		UHktWorldViewAnchorStrategy* Strategy = Cast<UHktWorldViewAnchorStrategy>(Element->AnchorStrategy);
		if (Strategy)
			Strategy->SetWorldState(CachedWorldState);
	}

	for (FHktEntityId EntityId : TrackedEntities)
	{
		UHktUIElement* Element = FindEntityElement(EntityId);
		if (!Element || !Element->View.IsValid()) continue;

		TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
		TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(SlateWidget);
		if (!EntityWidget.IsValid()) continue;

		int32 Health = CachedWorldState->GetProperty(EntityId, PropertyId::Health);
		int32 MaxHealth = CachedWorldState->GetProperty(EntityId, PropertyId::MaxHealth);
		int32 OwnerUid = CachedWorldState->GetProperty(EntityId, PropertyId::OwnedPlayerUid);
		int32 Team = CachedWorldState->GetProperty(EntityId, PropertyId::Team);

		EntityWidget->SetHealthPercent(MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 0.f);
		EntityWidget->SetOwnerLabel(OwnerUid != 0 ? FString::Printf(TEXT("P:%d"), OwnerUid) : TEXT("-"));

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
