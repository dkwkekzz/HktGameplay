// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktHUD.h"
#include "HktUISubsystem.h"
#include "HktUIElement.h"
#include "HktUITagDataAsset.h"
#include "HktUIAnchorStrategy.h"
#include "IHktUIView.h"
#include "HktAssetSubsystem.h"
#include "HktTagDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void AHktHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (PC)
	{
		UISubsystem = UHktUISubsystem::Get(PC);
	}
}

void AHktHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateEntityUI();
}

void AHktHUD::LoadAndCreateWidget(FGameplayTag WidgetTag, TFunction<void(UHktUIElement*)> OnCreated)
{
	if (!WidgetTag.IsValid()) return;
	if (!UISubsystem)
	{
		UISubsystem = UHktUISubsystem::Get(GetOwningPlayerController());
	}
	if (!UISubsystem) return;

	UHktAssetSubsystem* AssetSubsystem = UHktAssetSubsystem::Get(GetWorld());
	if (!AssetSubsystem) return;

	AssetSubsystem->LoadAssetAsync(WidgetTag, [this, OnCreated](UHktTagDataAsset* LoadedAsset)
	{
		UHktUITagDataAsset* UITagAsset = Cast<UHktUITagDataAsset>(LoadedAsset);
		if (!UITagAsset) return;

		TSharedPtr<IHktUIView> View = UITagAsset->CreateView();
		UHktUIAnchorStrategy* Strategy = UITagAsset->CreateStrategy(this);
		if (!View.IsValid() || !Strategy) return;

		UHktUIElement* Element = UISubsystem->CreateElement(View, Strategy, nullptr);
		if (OnCreated) OnCreated(Element);
	});
}

void AHktHUD::UpdateEntityUI()
{
	// WorldView가 설정된 경우 ForEachEntity 등으로 엔티티 UI 생성/제거/갱신.
	// FHktWorldView 타입은 HktRuntime에서 제공되며, 여기서는 스텁.
	if (!WorldView.IsValid()) return;
}
