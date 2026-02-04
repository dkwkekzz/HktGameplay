// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Managers/HktSelectionVisualManager.h"
#include "Settings/HktPresentationGlobalSetting.h"
#include "Actors/HktCharacter.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

FHktSelectionVisualManager::FHktSelectionVisualManager(UWorld* InWorld)
	: World(InWorld)
{
}

FHktSelectionVisualManager::~FHktSelectionVisualManager()
{
	ClearAll();
}

UMaterialInterface* FHktSelectionVisualManager::GetSelectionMaterial() const
{
	if (const UHktPresentationGlobalSetting* Settings = GetDefault<UHktPresentationGlobalSetting>())
	{
		return Settings->SelectionMaterial.LoadSynchronous();
	}
	return nullptr;
}

FLinearColor FHktSelectionVisualManager::GetSubjectColor() const
{
	if (const UHktPresentationGlobalSetting* Settings = GetDefault<UHktPresentationGlobalSetting>())
	{
		return Settings->SubjectSelectionColor;
	}
	return FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
}

FLinearColor FHktSelectionVisualManager::GetTargetColor() const
{
	if (const UHktPresentationGlobalSetting* Settings = GetDefault<UHktPresentationGlobalSetting>())
	{
		return Settings->TargetSelectionColor;
	}
	return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);
}

void FHktSelectionVisualManager::SetSelectedSubject(FHktEntityId EntityId, AHktCharacter* Character)
{
	// 이전 선택 해제
	ClearSubjectSelection();

	if (!Character || EntityId == InvalidEntityId)
	{
		return;
	}

	CurrentSubject = EntityId;
	SubjectCharacter = Character;

	// 데칼 생성
	if (GetSelectionMaterial())
	{
		SubjectDecal = CreateSelectionDecal(Character, GetSubjectColor());
	}

	UE_LOG(LogTemp, Verbose, TEXT("[SelectionVisualManager] Subject selected: Entity %d"), EntityId.RawValue);
}

void FHktSelectionVisualManager::ClearSubjectSelection()
{
	if (SubjectDecal.IsValid())
	{
		SubjectDecal->DestroyComponent();
		SubjectDecal = nullptr;
	}

	SubjectCharacter = nullptr;
	CurrentSubject = InvalidEntityId;
}

void FHktSelectionVisualManager::SetSelectedTarget(FHktEntityId EntityId, AHktCharacter* Character)
{
	// 이전 선택 해제
	ClearTargetSelection();

	if (!Character || EntityId == InvalidEntityId)
	{
		return;
	}

	// Subject와 같으면 표시하지 않음
	if (EntityId == CurrentSubject)
	{
		return;
	}

	CurrentTarget = EntityId;
	TargetCharacter = Character;

	// 데칼 생성
	if (GetSelectionMaterial())
	{
		TargetDecal = CreateSelectionDecal(Character, GetTargetColor());
	}

	UE_LOG(LogTemp, Verbose, TEXT("[SelectionVisualManager] Target selected: Entity %d"), EntityId.RawValue);
}

void FHktSelectionVisualManager::ClearTargetSelection()
{
	if (TargetDecal.IsValid())
	{
		TargetDecal->DestroyComponent();
		TargetDecal = nullptr;
	}

	TargetCharacter = nullptr;
	CurrentTarget = InvalidEntityId;
}

void FHktSelectionVisualManager::ClearAll()
{
	ClearSubjectSelection();
	ClearTargetSelection();
}

void FHktSelectionVisualManager::Tick(float DeltaTime)
{
	// Subject 데칼 업데이트
	if (SubjectDecal.IsValid() && SubjectCharacter.IsValid())
	{
		UpdateDecalAttachment(SubjectDecal.Get(), SubjectCharacter.Get());
	}
	else if (SubjectDecal.IsValid() && !SubjectCharacter.IsValid())
	{
		// Character가 파괴됨
		ClearSubjectSelection();
	}

	// Target 데칼 업데이트
	if (TargetDecal.IsValid() && TargetCharacter.IsValid())
	{
		UpdateDecalAttachment(TargetDecal.Get(), TargetCharacter.Get());
	}
	else if (TargetDecal.IsValid() && !TargetCharacter.IsValid())
	{
		// Character가 파괴됨
		ClearTargetSelection();
	}
}

UDecalComponent* FHktSelectionVisualManager::CreateSelectionDecal(AActor* AttachTo, FLinearColor Color)
{
	UMaterialInterface* SelectionMaterial = GetSelectionMaterial();
	if (!AttachTo || !SelectionMaterial)
	{
		return nullptr;
	}

	UDecalComponent* Decal = NewObject<UDecalComponent>(AttachTo);
	
	// 동적 머티리얼 인스턴스 생성 (색상 변경용)
	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(SelectionMaterial, AttachTo);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		Decal->SetDecalMaterial(DynamicMaterial);
	}
	else
	{
		Decal->SetDecalMaterial(SelectionMaterial);
	}

	Decal->DecalSize = DecalSize;
	Decal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Decal->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
	Decal->SetupAttachment(AttachTo->GetRootComponent());
	Decal->RegisterComponent();

	return Decal;
}

void FHktSelectionVisualManager::UpdateDecalAttachment(UDecalComponent* Decal, AActor* AttachTo)
{
	if (!Decal || !AttachTo)
	{
		return;
	}

	// 데칼이 다른 Actor에 붙어있으면 재부착
	if (Decal->GetAttachParent() != AttachTo->GetRootComponent())
	{
		Decal->AttachToComponent(AttachTo->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
}
