// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Managers/HktInteractionFXManager.h"
#include "Settings/HktPresentationGlobalSetting.h"
#include "HktCoreTypes.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

FHktInteractionFXManager::FHktInteractionFXManager(UWorld* InWorld)
	: World(InWorld)
{
}

FHktInteractionFXManager::~FHktInteractionFXManager()
{
	StopCurrentFX();
	HideTargetIndicator();
}

UNiagaraSystem* FHktInteractionFXManager::GetInteractionFXSystem() const
{
	if (const UHktPresentationGlobalSetting* Settings = GetDefault<UHktPresentationGlobalSetting>())
	{
		return Settings->InteractionFXSystem.LoadSynchronous();
	}
	return nullptr;
}

void FHktInteractionFXManager::SetTargetIndicatorMaterial(UMaterialInterface* InMaterial)
{
	TargetIndicatorMaterial = InMaterial;
}

void FHktInteractionFXManager::PlayIntentFX(const FHktIntentEvent& Event)
{
	if (Event.Location.IsZero())
	{
		return;
	}

	PlayFXAtLocation(Event.Location);

	UE_LOG(LogTemp, Verbose, TEXT("[InteractionFXManager] Playing Intent FX at %s for %s"),
		*Event.Location.ToString(), *Event.EventTag.ToString());
}

void FHktInteractionFXManager::PlayFXAtLocation(const FVector& Location)
{
	UNiagaraSystem* FXSystem = GetInteractionFXSystem();
	if (!World || !FXSystem)
	{
		return;
	}

	// 이전 FX 정리
	StopCurrentFX();

	// 새 FX 스폰
	ActiveFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		FXSystem,
		Location,
		FRotator::ZeroRotator,
		FVector::OneVector,
		true,  // bAutoDestroy
		true,  // bAutoActivate
		ENCPoolMethod::None,
		true   // bPreCullCheck
	);
}

void FHktInteractionFXManager::StopCurrentFX()
{
	if (ActiveFX.IsValid())
	{
		ActiveFX->DeactivateImmediate();
		ActiveFX = nullptr;
	}
}

void FHktInteractionFXManager::ShowTargetIndicator(const FVector& Location)
{
	if (!World)
	{
		return;
	}

	// 기존 인디케이터 재사용 또는 생성
	if (!TargetIndicator.IsValid())
	{
		// World에 임시 Actor 생성하여 데칼 부착
		AActor* TempActor = World->SpawnActor<AActor>();
		if (!TempActor)
		{
			return;
		}

		UDecalComponent* Decal = NewObject<UDecalComponent>(TempActor);
		
		if (TargetIndicatorMaterial.IsValid())
		{
			Decal->SetDecalMaterial(TargetIndicatorMaterial.Get());
		}

		Decal->DecalSize = TargetIndicatorSize;
		Decal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
		Decal->SetupAttachment(TempActor->GetRootComponent());
		Decal->RegisterComponent();

		TargetIndicator = Decal;
	}

	// 위치 설정
	if (TargetIndicator.IsValid())
	{
		AActor* Owner = TargetIndicator->GetOwner();
		if (Owner)
		{
			Owner->SetActorLocation(Location);
			Owner->SetActorHiddenInGame(false);
		}
	}
}

void FHktInteractionFXManager::HideTargetIndicator()
{
	if (TargetIndicator.IsValid())
	{
		AActor* Owner = TargetIndicator->GetOwner();
		if (Owner)
		{
			Owner->SetActorHiddenInGame(true);
		}
	}
}

void FHktInteractionFXManager::UpdateTargetIndicatorLocation(const FVector& Location)
{
	if (TargetIndicator.IsValid())
	{
		AActor* Owner = TargetIndicator->GetOwner();
		if (Owner)
		{
			Owner->SetActorLocation(Location);
		}
	}
}

void FHktInteractionFXManager::Tick(float DeltaTime)
{
	// FX 자동 정리는 Niagara 시스템이 처리
	// 추가 업데이트 로직이 필요하면 여기에 구현
}
