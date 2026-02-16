// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktIngameHUD.h"
#include "HktUISubsystem.h"
#include "HktUIElement.h"
#include "HktWorldViewAnchorStrategy.h"
#include "HktSlateView.h"
#include "Widgets/SHktIngameHudWidget.h"
#include "Widgets/SHktEntityHudWidget.h"
#include "HktGameplayTags.h"
#include "HktPropertyIds.h"
#include "HktUIHelpers.h"
#include "Components/HktClientSimulatorComponent.h"
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

void AHktIngameHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RefreshWorldView();
	if (bWorldViewValid)
	{
		UpdateEntityUI();
	}
}

void AHktIngameHUD::RefreshWorldView()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		bWorldViewValid = false;
		return;
	}

	UHktClientSimulatorComponent* ClientSim = HktUI::FindComponent<UHktClientSimulatorComponent>(PC);
	if (!ClientSim || !ClientSim->IsInitialized())
	{
		bWorldViewValid = false;
		return;
	}

	CachedWorldView.WorldState = &ClientSim->GetSimulationState();
	CachedWorldView.IntOverlays.Reset(); // 오버레이 없음 (커밋된 상태만 사용)
	bWorldViewValid = true;
}

void AHktIngameHUD::UpdateEntityUI()
{
	if (!bWorldViewValid || !CachedWorldView.WorldState) return;

	SyncEntityElements();
	UpdateEntityProperties();
}

void AHktIngameHUD::SyncEntityElements()
{
	if (!UISubsystem) return;

	const FHktWorldState* WS = CachedWorldView.WorldState;
	TSet<FHktEntityId> CurrentEntities;

	// 현재 존재하는 모든 엔티티 순회
	for (int32 SlotIndex = 0; SlotIndex < WS->IndexToEntity.Num(); ++SlotIndex)
	{
		FHktEntityId EntityId = WS->IndexToEntity[SlotIndex];
		if (EntityId == InvalidEntityId) continue;

		CurrentEntities.Add(EntityId);

		// 새 엔티티 → UI 생성
		if (!TrackedEntities.Contains(EntityId))
		{
			UHktUIElement* Element = UISubsystem->GetOrAddEntityElement(EntityId);
			if (Element && !Element->View.IsValid())
			{
				// SHktEntityHudWidget 직접 생성 (비동기 로드 없이 즉시)
				TSharedRef<SHktEntityHudWidget> EntityWidget = SNew(SHktEntityHudWidget);
				TSharedPtr<IHktUIView> View = MakeShared<FHktSlateView>(EntityWidget);

				// WorldViewAnchorStrategy 생성 및 설정
				UHktWorldViewAnchorStrategy* Strategy = NewObject<UHktWorldViewAnchorStrategy>(UISubsystem);
				Strategy->SetTargetEntity(EntityId, EntityHudOffset);
				Strategy->SetWorldView(&CachedWorldView);

				Element->InitializeElement(View, Strategy);
				UISubsystem->AddElementToCanvas(Element);

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
		}
	}

	// 삭제된 엔티티 감지 → UI 제거
	TSet<FHktEntityId> RemovedEntities = TrackedEntities.Difference(CurrentEntities);
	for (FHktEntityId RemovedId : RemovedEntities)
	{
		UISubsystem->RemoveEntityElement(RemovedId);
	}

	TrackedEntities = MoveTemp(CurrentEntities);
}

void AHktIngameHUD::UpdateEntityProperties()
{
	if (!UISubsystem) return;

	// WorldViewAnchorStrategy의 WorldView 포인터 갱신
	for (FHktEntityId EntityId : TrackedEntities)
	{
		UHktUIElement* Element = UISubsystem->FindEntityElement(EntityId);
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
		UHktUIElement* Element = UISubsystem->FindEntityElement(EntityId);
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
		UHktUIElement* Element = UISubsystem->FindEntityElement(EntityId);
		if (!Element || !Element->View.IsValid()) return;

		TSharedRef<SWidget> SlateWidget = Element->View->GetSlateWidget();
		TSharedPtr<SHktEntityHudWidget> EntityWidget = StaticCastSharedRef<SHktEntityHudWidget>(SlateWidget);
		if (!EntityWidget.IsValid()) return;

		EntityWidget->SetOwnerLabel(Value != 0 ? FString::Printf(TEXT("P:%d"), Value) : TEXT("-"));
	});

	// Team 변경 반영
	CachedWorldView.ForEachDirtyEntity(PropertyId::Team, [this](FHktEntityId EntityId, int32 Value)
	{
		UHktUIElement* Element = UISubsystem->FindEntityElement(EntityId);
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
