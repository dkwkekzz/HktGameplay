// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXRenderer.h"
#include "HktVFXAssetBank.h"
#include "HktVFXIntent.h"
#include "HktAssetSubsystem.h"
#include "DataAssets/HktVFXVisualDataAsset.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogHktVFXRenderer, Log, All);

FHktVFXRenderer::FHktVFXRenderer(ULocalPlayer* InLP)
	: LocalPlayer(InLP)
{
}

void FHktVFXRenderer::SetAssetBank(UHktVFXAssetBank* InBank)
{
	AssetBank = InBank;
}

void FHktVFXRenderer::SetFallbackSystem(UNiagaraSystem* InSystem)
{
	FallbackSystem = InSystem;
}

void FHktVFXRenderer::PlayVFXAtLocation(FGameplayTag VFXTag, FVector Location)
{
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogHktVFXRenderer, Warning, TEXT("PlayVFXAtLocation: No world"));
		return;
	}

	UHktAssetSubsystem* AssetSubsystem = UHktAssetSubsystem::Get(World);
	if (!AssetSubsystem)
	{
		UE_LOG(LogHktVFXRenderer, Warning, TEXT("PlayVFXAtLocation: No AssetSubsystem"));
		return;
	}

	AssetSubsystem->LoadAssetAsync(VFXTag, [this, VFXTag, Location, World](UHktTagDataAsset* LoadedAsset)
	{
		UHktVFXVisualDataAsset* VFXAsset = Cast<UHktVFXVisualDataAsset>(LoadedAsset);
		if (!VFXAsset)
		{
			UE_LOG(LogHktVFXRenderer, Warning, TEXT("PlayVFXAtLocation: No UHktVFXVisualDataAsset for tag [%s]"), *VFXTag.ToString());
			return;
		}

		UNiagaraSystem* System = VFXAsset->NiagaraSystem.LoadSynchronous();
		if (!System)
		{
			UE_LOG(LogHktVFXRenderer, Warning, TEXT("PlayVFXAtLocation: NiagaraSystem not set for tag [%s]"), *VFXTag.ToString());
			return;
		}

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			System,
			Location,
			FRotator::ZeroRotator,
			FVector::OneVector,
			true,   // bAutoDestroy
			true,   // bAutoActivate
			ENCPoolMethod::AutoRelease);

		UE_LOG(LogHktVFXRenderer, Verbose, TEXT("PlayVFXAtLocation: [%s] at %s"), *VFXTag.ToString(), *Location.ToString());
	});
}

void FHktVFXRenderer::PlayVFXWithIntent(const FHktVFXIntent& Intent)
{
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogHktVFXRenderer, Warning, TEXT("PlayVFXWithIntent: No world"));
		return;
	}

	UNiagaraSystem* System = nullptr;
	if (AssetBank)
	{
		System = AssetBank->FindClosestSystem(Intent);
	}

	if (!System)
	{
		System = FallbackSystem;
	}

	if (!System)
	{
		UE_LOG(LogHktVFXRenderer, Warning, TEXT("PlayVFXWithIntent: No system for [%s]"), *Intent.GetAssetKey());
		return;
	}

	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		System,
		Intent.Location,
		Intent.Direction.Rotation(),
		FVector::OneVector,
		true,   // bAutoDestroy
		true,   // bAutoActivate
		ENCPoolMethod::AutoRelease);

	if (Comp)
	{
		ApplyRuntimeOverrides(Comp, Intent);
	}

	UE_LOG(LogHktVFXRenderer, Verbose, TEXT("PlayVFXWithIntent: [%s] at %s"), *Intent.GetAssetKey(), *Intent.Location.ToString());
}

void FHktVFXRenderer::ApplyRuntimeOverrides(UNiagaraComponent* Comp, const FHktVFXIntent& Intent)
{
	float RadiusScale = Intent.Radius / 200.f;
	Comp->SetVariableFloat(FName("RadiusScale"), RadiusScale);
	Comp->SetVariableFloat(FName("IntensityMult"), Intent.Intensity);

	if (Intent.Duration > 0.f)
	{
		Comp->SetVariableFloat(FName("DurationScale"), Intent.Duration);
	}

	Comp->SetVariableFloat(FName("PowerLevel"), Intent.SourcePower);

	FLinearColor ElementTint = GetElementTintColor(Intent.Element);
	Comp->SetVariableLinearColor(FName("ElementTint"), ElementTint);

	float OverallScale = FMath::Lerp(0.5f, 2.0f, Intent.Intensity);
	Comp->SetWorldScale3D(FVector(OverallScale * RadiusScale));
}

FLinearColor FHktVFXRenderer::GetElementTintColor(EHktVFXElement Element)
{
	switch (Element)
	{
	case EHktVFXElement::Fire:      return FLinearColor(1.0f, 0.6f, 0.2f, 1.0f);
	case EHktVFXElement::Ice:       return FLinearColor(0.5f, 0.8f, 1.0f, 1.0f);
	case EHktVFXElement::Lightning: return FLinearColor(0.7f, 0.9f, 1.0f, 1.0f);
	case EHktVFXElement::Water:     return FLinearColor(0.3f, 0.5f, 0.9f, 1.0f);
	case EHktVFXElement::Earth:     return FLinearColor(0.6f, 0.4f, 0.2f, 1.0f);
	case EHktVFXElement::Wind:      return FLinearColor(0.8f, 0.9f, 0.8f, 1.0f);
	case EHktVFXElement::Dark:      return FLinearColor(0.4f, 0.1f, 0.6f, 1.0f);
	case EHktVFXElement::Holy:      return FLinearColor(1.0f, 0.95f, 0.7f, 1.0f);
	case EHktVFXElement::Poison:    return FLinearColor(0.3f, 0.8f, 0.2f, 1.0f);
	case EHktVFXElement::Arcane:    return FLinearColor(0.5f, 0.3f, 0.9f, 1.0f);
	case EHktVFXElement::Nature:    return FLinearColor(0.4f, 0.8f, 0.3f, 1.0f);
	default:                        return FLinearColor::White;
	}
}

void FHktVFXRenderer::Teardown()
{
	AssetBank = nullptr;
	FallbackSystem = nullptr;
}
