// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Managers/HktEntityHUDManager.h"
#include "Settings/HktPresentationGlobalSetting.h"
#include "Widgets/HktEntityHUDWidget.h"
#include "Actors/HktCharacter.h"
#include "HktCoreInterfaces.h"
#include "HktPropertyIds.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"

FHktEntityHUDManager::FHktEntityHUDManager(UWorld* InWorld)
	: World(InWorld)
{
}

FHktEntityHUDManager::~FHktEntityHUDManager()
{
	// 모든 HUD 정리
	for (auto& Pair : EntityHUDMap)
	{
		if (UWidgetComponent* Widget = Pair.Value.WidgetComponent.Get())
		{
			Widget->DestroyComponent();
		}
	}
	EntityHUDMap.Empty();
}

TSubclassOf<UUserWidget> FHktEntityHUDManager::GetHUDWidgetClass() const
{
	if (const UHktPresentationGlobalSetting* Settings = GetDefault<UHktPresentationGlobalSetting>())
	{
		return Settings->HUDWidgetClass.LoadSynchronous();
	}
	return nullptr;
}

void FHktEntityHUDManager::AddEntityHUD(FHktEntityId EntityId, AHktCharacter* Character)
{
	TSubclassOf<UUserWidget> HUDWidgetClass = GetHUDWidgetClass();
	if (!Character || !HUDWidgetClass || EntityId == InvalidEntityId)
	{
		return;
	}

	// 이미 존재하면 스킵
	if (EntityHUDMap.Contains(EntityId.RawValue))
	{
		return;
	}

	// WidgetComponent 생성
	UWidgetComponent* WidgetComp = NewObject<UWidgetComponent>(Character);
	WidgetComp->SetupAttachment(Character->GetRootComponent());
	WidgetComp->SetRelativeLocation(HUDOffset);
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComp->SetWidgetClass(HUDWidgetClass);
	WidgetComp->SetDrawSize(HUDDrawSize);
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComp->RegisterComponent();

	// 위젯 획득 및 설정
	UHktEntityHUDWidget* HUDWidget = Cast<UHktEntityHUDWidget>(WidgetComp->GetWidget());
	if (HUDWidget)
	{
		HUDWidget->SetShowEntityId(bShowEntityId);
		HUDWidget->SetShowHealthBar(bShowHealthBar);
		HUDWidget->SetShowManaBar(bShowManaBar);
	}

	// 맵에 추가
	FEntityHUDEntry Entry;
	Entry.WidgetComponent = WidgetComp;
	Entry.Widget = HUDWidget;
	EntityHUDMap.Add(EntityId.RawValue, Entry);

	UE_LOG(LogTemp, Verbose, TEXT("[EntityHUDManager] Added HUD for Entity %d"), EntityId.RawValue);
}

void FHktEntityHUDManager::RemoveEntityHUD(FHktEntityId EntityId)
{
	FEntityHUDEntry* Entry = EntityHUDMap.Find(EntityId.RawValue);
	if (!Entry)
	{
		return;
	}

	if (UWidgetComponent* Widget = Entry->WidgetComponent.Get())
	{
		Widget->DestroyComponent();
	}

	EntityHUDMap.Remove(EntityId.RawValue);

	UE_LOG(LogTemp, Verbose, TEXT("[EntityHUDManager] Removed HUD for Entity %d"), EntityId.RawValue);
}

bool FHktEntityHUDManager::HasEntityHUD(FHktEntityId EntityId) const
{
	return EntityHUDMap.Contains(EntityId.RawValue);
}

void FHktEntityHUDManager::UpdateEntityHUD(FHktEntityId EntityId, const FHktEntityHUDData& Data)
{
	FEntityHUDEntry* Entry = EntityHUDMap.Find(EntityId.RawValue);
	if (!Entry)
	{
		return;
	}

	UHktEntityHUDWidget* Widget = Entry->Widget.Get();
	if (Widget)
	{
		Widget->UpdateHUDData(Data);
	}
}

void FHktEntityHUDManager::Tick(float DeltaTime, IHktStashInterface* Stash)
{
	if (!Stash)
	{
		return;
	}

	for (auto& Pair : EntityHUDMap)
	{
		UHktEntityHUDWidget* Widget = Pair.Value.Widget.Get();
		if (!Widget)
		{
			continue;
		}

		FHktEntityId EntityId(Pair.Key);
		if (!Stash->IsValidEntity(EntityId))
		{
			continue;
		}

		FHktEntityHUDData Data = BuildHUDDataFromStash(EntityId, Stash);
		Widget->UpdateHUDData(Data);
	}
}

void FHktEntityHUDManager::SetShowEntityId(bool bShow)
{
	bShowEntityId = bShow;

	for (auto& Pair : EntityHUDMap)
	{
		if (UHktEntityHUDWidget* Widget = Pair.Value.Widget.Get())
		{
			Widget->SetShowEntityId(bShow);
		}
	}
}

void FHktEntityHUDManager::SetShowHealthBar(bool bShow)
{
	bShowHealthBar = bShow;

	for (auto& Pair : EntityHUDMap)
	{
		if (UHktEntityHUDWidget* Widget = Pair.Value.Widget.Get())
		{
			Widget->SetShowHealthBar(bShow);
		}
	}
}

void FHktEntityHUDManager::SetShowManaBar(bool bShow)
{
	bShowManaBar = bShow;

	for (auto& Pair : EntityHUDMap)
	{
		if (UHktEntityHUDWidget* Widget = Pair.Value.Widget.Get())
		{
			Widget->SetShowManaBar(bShow);
		}
	}
}

UHktEntityHUDWidget* FHktEntityHUDManager::GetHUDWidget(FHktEntityId EntityId) const
{
	const FEntityHUDEntry* Entry = EntityHUDMap.Find(EntityId.RawValue);
	if (Entry)
	{
		return Entry->Widget.Get();
	}
	return nullptr;
}

FHktEntityHUDData FHktEntityHUDManager::BuildHUDDataFromStash(FHktEntityId EntityId, IHktStashInterface* Stash) const
{
	FHktEntityHUDData Data;
	Data.EntityId = EntityId;
	Data.Health = Stash->GetProperty(EntityId, PropertyId::Health);
	Data.MaxHealth = Stash->GetProperty(EntityId, PropertyId::MaxHealth);
	Data.Mana = Stash->GetProperty(EntityId, PropertyId::Mana);
	Data.MaxMana = Stash->GetProperty(EntityId, PropertyId::MaxMana);

	// MaxHealth가 0이면 기본값 설정
	if (Data.MaxHealth <= 0)
	{
		Data.MaxHealth = 100;
	}
	if (Data.MaxMana <= 0)
	{
		Data.MaxMana = 100;
	}

	return Data;
}
