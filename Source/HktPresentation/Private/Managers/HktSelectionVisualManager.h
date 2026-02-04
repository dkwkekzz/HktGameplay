// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktRuntimeTypes.h"

class UWorld;
class UMaterialInterface;
class UDecalComponent;
class AHktCharacter;

/**
 * FHktSelectionVisualManager
 * 
 * 선택 시각화 관리
 * - Subject 선택 시 데칼 표시
 * - Target 선택 시 별도 색상 데칼 표시
 */
class HKTPRESENTATION_API FHktSelectionVisualManager
{
public:
	FHktSelectionVisualManager(UWorld* InWorld);
	~FHktSelectionVisualManager();

	// === 설정 ===
	
	void SetDecalSize(FVector Size) { DecalSize = Size; }

	// === Subject 선택 ===
	
	/** Subject 선택 설정 */
	void SetSelectedSubject(FHktEntityId EntityId, AHktCharacter* Character);
	
	/** Subject 선택 해제 */
	void ClearSubjectSelection();
	
	/** 현재 Subject EntityId */
	FHktEntityId GetSelectedSubject() const { return CurrentSubject; }

	// === Target 선택 ===
	
	/** Target 선택 설정 */
	void SetSelectedTarget(FHktEntityId EntityId, AHktCharacter* Character);
	
	/** Target 선택 해제 */
	void ClearTargetSelection();
	
	/** 현재 Target EntityId */
	FHktEntityId GetSelectedTarget() const { return CurrentTarget; }

	// === 전체 해제 ===
	
	void ClearAll();

	// === 업데이트 ===
	
	/** 매 틱 데칼 위치 업데이트 (Character 이동 시) */
	void Tick(float DeltaTime);

private:
	UDecalComponent* CreateSelectionDecal(AActor* AttachTo, FLinearColor Color);
	void UpdateDecalAttachment(UDecalComponent* Decal, AActor* AttachTo);

	UMaterialInterface* GetSelectionMaterial() const;
	FLinearColor GetSubjectColor() const;
	FLinearColor GetTargetColor() const;

private:
	UWorld* World = nullptr;
	FVector DecalSize = FVector(64.0f, 64.0f, 64.0f);

	// Subject 선택
	TWeakObjectPtr<UDecalComponent> SubjectDecal;
	TWeakObjectPtr<AHktCharacter> SubjectCharacter;
	FHktEntityId CurrentSubject;

	// Target 선택
	TWeakObjectPtr<UDecalComponent> TargetDecal;
	TWeakObjectPtr<AHktCharacter> TargetCharacter;
	FHktEntityId CurrentTarget;
};
