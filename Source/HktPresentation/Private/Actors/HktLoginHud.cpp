// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktLoginHud.h"
#include "Slates/SHktLoginWidget.h"
#include "HktUserEventConsumer.h"
#include "Settings/HktPresentationGlobalSetting.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"

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

	TOptional<FSlateBrush> BackgroundBrush;
	const UHktPresentationGlobalSetting* Settings = GetDefault<UHktPresentationGlobalSetting>();
	if (Settings && Settings->LoginBackgroundTexture.IsValid())
	{
		if (UTexture2D* Tex = Settings->LoginBackgroundTexture.LoadSynchronous())
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Tex);
			Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
			Brush.DrawAs = ESlateBrushDrawType::Image;
			BackgroundBrush = Brush;
		}
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

	LoginWidgetSlate = SNew(SHktLoginWidget)
		.OnLoginRequested(OnLogin)
		.BackgroundBrush(BackgroundBrush);

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
