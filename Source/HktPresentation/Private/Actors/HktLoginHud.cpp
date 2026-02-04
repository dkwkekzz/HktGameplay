// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktLoginHud.h"
#include "HktUserEventConsumer.h"
#include "DataAssets/HktWidgetLoginHudDataAsset.h"
#include "HktAssetSubsystem.h"
#include "HktGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/Optional.h"

AHktLoginHud::AHktLoginHud()
{
}

void AHktLoginHud::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwningPlayerController() || !GetOwningPlayerController()->IsLocalController())
	{
		return;
	}

	AddLoginWidgetToViewport();
}

void AHktLoginHud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveLoginWidgetFromViewport();
	Super::EndPlay(EndPlayReason);
}

void AHktLoginHud::AddLoginWidgetToViewport()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}

	FOnHktLoginRequested OnLogin;
	OnLogin.BindLambda([this](const FString& ID, const FString& PW)
	{
		if (IHktUserEventConsumer* Consumer = Cast<IHktUserEventConsumer>(GetOwningPlayerController()))
		{
			FHktUserEvent Event(FName("Click_Login"));
			Event.Datas.Add(ID);
			Event.Datas.Add(PW);
			Consumer->OnUserEvent(Event);
		}
	});

	UHktAssetSubsystem* Subsystem = UHktAssetSubsystem::Get(GetWorld());
	if (!Subsystem)
	{
		return;
	}
	
	Subsystem->LoadAssetAsync(HktGameplayTags::Widget_LoginHud, [this, OnLogin, Subsystem](UHktTagDataAsset* Asset)
	{
		UHktWidgetLoginHudDataAsset* Config = Cast<UHktWidgetLoginHudDataAsset>(Asset);
		if (!Config)
		{
			return;
		}

		TOptional<FSlateBrush> BackgroundBrush;
		if (UTexture2D* Tex = Config->LoginBackgroundTexture)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Tex);
			Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
			Brush.DrawAs = ESlateBrushDrawType::Image;
			BackgroundBrush = Brush;
		}

		CreateAndAddLoginWidget(OnLogin, BackgroundBrush, Config);
	});
}

void AHktLoginHud::CreateAndAddLoginWidget(
	const FOnHktLoginRequested& OnLogin,
	const TOptional<FSlateBrush>& BackgroundBrush,
	UHktWidgetLoginHudDataAsset* LoginWidgetDataAsset)
{
	if (!GEngine || !GEngine->GameViewport) { return; }

	LoginWidgetSlate = SNew(SHktLoginHudWidget)
		.OnLoginRequested(OnLogin)
		.BackgroundBrush(BackgroundBrush)
		.LoginWidgetDataAsset(LoginWidgetDataAsset);

	GEngine->GameViewport->AddViewportWidgetContent(LoginWidgetSlate.ToSharedRef());
}

void AHktLoginHud::RemoveLoginWidgetFromViewport()
{
	if (LoginWidgetSlate.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(LoginWidgetSlate.ToSharedRef());
		LoginWidgetSlate.Reset();
	}
}
