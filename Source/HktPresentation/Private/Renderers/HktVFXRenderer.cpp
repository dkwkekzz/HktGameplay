// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXRenderer.h"
#include "HktVFXAssetBank.h"
#include "HktVFXIntent.h"
#include "HktAssetSubsystem.h"
#include "HktPresentationState.h"
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

// ============================================================================
// Convention Path → NiagaraSystem 직접 로드 (DataAsset 불필요)
// ============================================================================

UNiagaraSystem* FHktVFXRenderer::ResolveNiagaraSystem(FGameplayTag VFXTag)
{
	// 캐시 확인
	if (TWeakObjectPtr<UNiagaraSystem>* Cached = NiagaraSystemCache.Find(VFXTag))
	{
		if (Cached->IsValid())
		{
			return Cached->Get();
		}
		NiagaraSystemCache.Remove(VFXTag);
	}

	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!World) return nullptr;

	UHktAssetSubsystem* AssetSubsystem = UHktAssetSubsystem::Get(World);
	if (!AssetSubsystem) return nullptr;

	// Convention Path로 UObject 직접 로드 → UNiagaraSystem 캐스트
	UObject* Loaded = AssetSubsystem->LoadByConventionSync(VFXTag);
	UNiagaraSystem* System = Cast<UNiagaraSystem>(Loaded);

	if (System)
	{
		NiagaraSystemCache.Add(VFXTag, System);
		UE_LOG(LogHktVFXRenderer, Verbose, TEXT("ResolveNiagaraSystem: [%s] → %s"), *VFXTag.ToString(), *System->GetPathName());
	}
	else
	{
		UE_LOG(LogHktVFXRenderer, Warning, TEXT("ResolveNiagaraSystem: Failed for tag [%s]"), *VFXTag.ToString());
	}

	return System;
}

// ============================================================================
// Tag 기반 VFX 재생 (Convention Path → NiagaraSystem 직접)
// ============================================================================

void FHktVFXRenderer::PlayVFXAtLocation(FGameplayTag VFXTag, FVector Location)
{
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogHktVFXRenderer, Warning, TEXT("PlayVFXAtLocation: No world"));
		return;
	}

	UNiagaraSystem* System = ResolveNiagaraSystem(VFXTag);
	if (!System)
	{
		UE_LOG(LogHktVFXRenderer, Warning, TEXT("PlayVFXAtLocation: No NiagaraSystem for tag [%s]"), *VFXTag.ToString());
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
}

// ============================================================================
// Intent 기반 VFX 재생 (AssetBank 퍼지 매칭)
// ============================================================================

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

// ============================================================================
// 엔터티 부착 지속형 VFX
// ============================================================================

void FHktVFXRenderer::AttachVFXToEntity(FGameplayTag VFXTag, FHktEntityId EntityId, FVector Location)
{
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!World) return;

	FEntityVFXKey Key{ VFXTag, EntityId };

	// 기존 VFX가 있으면 제거
	if (TWeakObjectPtr<UNiagaraComponent>* Existing = EntityVFXMap.Find(Key))
	{
		if (Existing->IsValid())
		{
			Existing->Get()->DestroyComponent();
		}
		EntityVFXMap.Remove(Key);
	}

	UNiagaraSystem* System = ResolveNiagaraSystem(VFXTag);
	if (!System)
	{
		UE_LOG(LogHktVFXRenderer, Warning, TEXT("AttachVFXToEntity: No NiagaraSystem for tag [%s]"), *VFXTag.ToString());
		return;
	}

	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		System,
		Location,
		FRotator::ZeroRotator,
		FVector::OneVector,
		false,  // bAutoDestroy — 지속형이므로 수동 관리
		true,   // bAutoActivate
		ENCPoolMethod::None);

	if (Comp)
	{
		EntityVFXMap.Add(Key, Comp);
		UE_LOG(LogHktVFXRenderer, Verbose, TEXT("AttachVFXToEntity: [%s] on Entity=%d at %s"), *VFXTag.ToString(), EntityId, *Location.ToString());
	}
}

void FHktVFXRenderer::DetachVFXFromEntity(FGameplayTag VFXTag, FHktEntityId EntityId)
{
	FEntityVFXKey Key{ VFXTag, EntityId };
	if (TWeakObjectPtr<UNiagaraComponent>* Found = EntityVFXMap.Find(Key))
	{
		if (Found->IsValid())
		{
			Found->Get()->DestroyComponent();
		}
		EntityVFXMap.Remove(Key);
		UE_LOG(LogHktVFXRenderer, Verbose, TEXT("DetachVFXFromEntity: [%s] from Entity=%d"), *VFXTag.ToString(), EntityId);
	}
}

void FHktVFXRenderer::UpdateEntityVFXPositions(const FHktPresentationState& State)
{
	for (auto It = EntityVFXMap.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}

		const FHktEntityPresentation* Entity = State.Get(It.Key().EntityId);
		if (!Entity || !Entity->IsAlive())
		{
			It.Value().Get()->DestroyComponent();
			It.RemoveCurrent();
			continue;
		}

		FVector Pos = Entity->Location.Get();
		It.Value().Get()->SetWorldLocation(Pos);
	}
}

void FHktVFXRenderer::Teardown()
{
	// 지속형 VFX 정리
	for (auto& Pair : EntityVFXMap)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value.Get()->DestroyComponent();
		}
	}
	EntityVFXMap.Empty();
	NiagaraSystemCache.Empty();

	AssetBank = nullptr;
	FallbackSystem = nullptr;
}
