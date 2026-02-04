// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktPresentationTypes.h"

class UWorld;
class UNiagaraSystem;
class UNiagaraComponent;
class UDecalComponent;
struct FHktIntentEvent;

/**
 * FHktInteractionFXManager
 * 
 * 인터랙션 이펙트 관리
 * - 클릭 위치 이펙트
 * - 이동 명령 경로 표시
 * - 스킬 타겟 범위 표시
 */
class HKTPRESENTATION_API FHktInteractionFXManager
{
public:
	FHktInteractionFXManager(UWorld* InWorld);
	~FHktInteractionFXManager();

	// === 설정 ===
	
	void SetTargetIndicatorMaterial(UMaterialInterface* InMaterial);
	void SetTargetIndicatorSize(FVector Size) { TargetIndicatorSize = Size; }

	// === Intent FX ===
	
	/** Intent 제출 시 이펙트 재생 */
	void PlayIntentFX(const FHktIntentEvent& Event);
	
	/** 특정 위치에 이펙트 재생 */
	void PlayFXAtLocation(const FVector& Location);
	
	/** 현재 재생 중인 FX 중지 */
	void StopCurrentFX();

	// === 타겟 인디케이터 ===
	
	/** 타겟 위치 표시 (미리보기) */
	void ShowTargetIndicator(const FVector& Location);
	
	/** 타겟 인디케이터 숨기기 */
	void HideTargetIndicator();
	
	/** 타겟 인디케이터 위치 업데이트 */
	void UpdateTargetIndicatorLocation(const FVector& Location);

	// === 업데이트 ===
	
	void Tick(float DeltaTime);

private:
	UNiagaraSystem* GetInteractionFXSystem() const;

private:
	UWorld* World = nullptr;
	TWeakObjectPtr<UMaterialInterface> TargetIndicatorMaterial;
	
	FVector TargetIndicatorSize = FVector(50.0f, 50.0f, 50.0f);

	// 현재 재생 중인 FX
	TWeakObjectPtr<UNiagaraComponent> ActiveFX;
	
	// 타겟 인디케이터
	TWeakObjectPtr<UDecalComponent> TargetIndicator;
};
