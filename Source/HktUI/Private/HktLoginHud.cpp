// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktLoginHud.h"
#include "HktUISubsystem.h"
#include "HktEventParam.h"
#include "HktAssetSubsystem.h"
#include "HktGameplayTags.h"
#include "DataAssets/HktWidgetLoginHudDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

AHktLoginHud::AHktLoginHud() {}

void AHktLoginHud::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwningPlayerController() || !GetOwningPlayerController()->IsLocalController()) { return; }

	UISubsystem = UHktUISubsystem::Get(GetOwningPlayerController());
	AddLoginWidgetToViewport();
}

void AHktLoginHud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveLoginWidgetFromViewport();
	UISubsystem = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AHktLoginHud::AddLoginWidgetToViewport()
{
	if (!GEngine || !GEngine->GameViewport) { return; }

	UHktAssetSubsystem* AssetSub = UHktAssetSubsystem::Get(GetWorld());
	if (!AssetSub) { return; }

	AssetSub->LoadAssetAsync(HktGameplayTags::Widget_LoginHud, [this](UHktTagDataAsset* Asset)
	{
		UHktWidgetLoginHudDataAsset* Config = Cast<UHktWidgetLoginHudDataAsset>(Asset);
		if (!Config) { return; }

		TOptional<FSlateBrush> BackgroundBrush;
		if (UTexture2D* Tex = Config->LoginBackgroundTexture)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Tex);
			Brush.ImageSize = FVector2D(Tex->GetSizeX(), Tex->GetSizeY());
			Brush.DrawAs = ESlateBrushDrawType::Image;
			BackgroundBrush = Brush;
		}
		CreateAndAddLoginWidget(BackgroundBrush, Config);
	});
}

void AHktLoginHud::CreateAndAddLoginWidget(const TOptional<FSlateBrush>& BackgroundBrush, UHktWidgetLoginHudDataAsset* DataAsset)
{
	if (!GEngine || !GEngine->GameViewport) { return; }

	FOnHktLoginRequested OnLogin;
	OnLogin.BindRaw(this, &AHktLoginHud::OnLoginRequested);

	LoginWidgetSlate = SNew(SHktLoginHudWidget)
		.OnLoginRequested(OnLogin)
		.BackgroundBrush(BackgroundBrush)
		.LoginWidgetDataAsset(DataAsset);

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

void AHktLoginHud::OnLoginRequested(const FString& ID, const FString& PW)
{
	UHktUISubsystem* Sub = UISubsystem.Get();
	if (!Sub) { return; }

	UHktLoginEventParam* Param = NewObject<UHktLoginEventParam>(Sub);
	Param->UserId = ID;
	Param->Password = PW;
	Param->OnCompleted.AddDynamic(this, &AHktLoginHud::OnLoginCompleted);

	Sub->DispatchToPlayerController(HktGameplayTags::Event_Login, Param);
}

void AHktLoginHud::OnLoginCompleted(UHktEventParam* Param, bool bSuccess)
{
	if (bSuccess)
	{
		RemoveLoginWidgetFromViewport();
		UE_LOG(LogTemp, Log, TEXT("[HktLoginHud] Login succeeded"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktLoginHud] Login failed: %s"),
			Param ? *Param->GetErrorMessage() : TEXT("Unknown"));
	}
}
