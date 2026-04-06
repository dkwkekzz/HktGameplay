// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktUnitActor.h"
#include "Deconstruct/HktDeconstructTypes.h"
#include "HktDeconstructUnitActor.generated.h"

class UNiagaraComponent;
class UHktDeconstructParamController;
class UHktDeconstructVisualDataAsset;

/**
 * Skeletal Mesh Deconstruction 비주얼 Actor.
 *
 * AHktUnitActor를 상속하여 NiagaraComponent(NS_HktDeconstruct)를 추가하고,
 * VM State(Health, Element, Combat State)를 Niagara User Parameter로 변환한다.
 *
 * 설계 원칙: AHktUnitActor처럼 순수 리액티브.
 * - ApplyPresentation()에서 TargetParams만 설정
 * - Tick()에서 FInterpTo로 CurrentParams → TargetParams 수렴
 * - 자체 상태 머신/타이머 없음. 모든 전환은 보간 속도만으로 제어.
 */
UCLASS(Blueprintable)
class AHktDeconstructUnitActor : public AHktUnitActor
{
	GENERATED_BODY()

public:
	AHktDeconstructUnitActor();

	virtual void ApplyPresentation(const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll,
		TFunctionRef<AActor*(FHktEntityId)> GetActorFunc) override;

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	void InitializeDeconstruct(const UHktDeconstructVisualDataAsset* InDataAsset);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Deconstruct")
	TObjectPtr<UHktDeconstructVisualDataAsset> DeconstructDataAsset;

	UPROPERTY(VisibleAnywhere, Category = "HKT|Deconstruct")
	TObjectPtr<UNiagaraComponent> DeconstructNiagaraComponent;

private:
	// --- 보간 속도 (높을수록 빠르게 수렴) ---
	static constexpr float InterpSpeed_Coherence = 2.0f;
	static constexpr float InterpSpeed_Scatter = 2.0f;
	static constexpr float InterpSpeed_Agitation = 4.0f;
	static constexpr float InterpSpeed_Multipliers = 6.0f;
	static constexpr float InterpSpeed_AgitationDecay = 2.0f;

	// --- 매핑 상수 ---
	static constexpr float MaxPointScatter = 50.0f;
	static constexpr float MinPointDensity = 0.3f;
	static constexpr float DamageToAgitationScale = 5.0f;
	static constexpr float MaxAgitationFromMovement = 0.3f;
	static constexpr float MovementSpeedRef = 600.0f;

	// --- 스킬 스파이크 값 ---
	static constexpr float SkillRibbonWidthMult = 3.0f;
	static constexpr float SkillRibbonEmissiveMult = 5.0f;
	static constexpr float SkillFragmentScaleMult = 2.0f;
	static constexpr float SkillAuraVelMult = 3.0f;

	// --- 사망 연출 값 ---
	static constexpr float DeathAuraSpawnMult = 3.0f;
	static constexpr float DeathAuraVelMult = 2.0f;

	UPROPERTY()
	TObjectPtr<UHktDeconstructParamController> ParamController;

	EHktDeconstructElement CurrentElement = EHktDeconstructElement::Fire;

	FHktDeconstructParams CurrentParams;
	FHktDeconstructParams TargetParams;
	FHktDeconstructParams LastPushedParams;

	float PrevHealthRatio = 1.0f;
	bool bDeconstructInitialized = false;
	bool bParamsDirty = true;
	bool bDead = false;

	void UpdateElement(EHktDeconstructElement NewElement);
};
