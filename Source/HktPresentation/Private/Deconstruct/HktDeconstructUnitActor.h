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
 * VM State(Health, Element, Combat State)를 Niagara User Parameter로 변환하여
 * "점/결정체/에너지로 해체-재구성되는" 비주얼을 실현한다.
 *
 * 기존 FHktActorRenderer 파이프라인이 자동으로 스폰/관리한다.
 * UHktActorVisualDataAsset::ActorClass에 이 클래스를 지정하면 된다.
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
	// --- 튜닝 상수 ---
	static constexpr float MaxPointScatter = 50.0f;
	static constexpr float MinPointDensity = 0.3f;
	static constexpr float DamageToAgitationScale = 5.0f;   // 데미지 비율 → Agitation 변환 계수
	static constexpr float MaxAgitationFromMovement = 0.3f;
	static constexpr float MovementSpeedRef = 600.0f;        // 이 속도에서 Agitation=MaxAgitationFromMovement
	static constexpr float SpawnAnimDuration = 1.5f;
	static constexpr float DeathAnimDuration = 2.0f;
	static constexpr float SkillSpikeDuration = 0.5f;
	static constexpr float PostSpawnAgitation = 0.2f;
	static constexpr float DeathAuraSpawnMult = 3.0f;
	static constexpr float DeathAuraVelMult = 2.0f;
	static constexpr float SkillRibbonWidthMult = 3.0f;
	static constexpr float SkillRibbonEmissiveMult = 5.0f;
	static constexpr float SkillFragmentScaleMult = 2.0f;
	static constexpr float SkillAuraVelMult = 3.0f;

	UPROPERTY()
	TObjectPtr<UHktDeconstructParamController> ParamController;

	EHktDeconstructElement CurrentElement = EHktDeconstructElement::Fire;

	FHktDeconstructParams CurrentParams;
	FHktDeconstructParams TargetParams;
	FHktDeconstructParams LastPushedParams;

	float PrevHealthRatio = 1.0f;
	bool bDeconstructInitialized = false;
	bool bParamsDirty = true;

	bool bSpawnAnimating = false;
	float SpawnAnimElapsed = 0.0f;

	bool bDeathAnimating = false;
	float DeathAnimElapsed = 0.0f;

	bool bSkillSpiking = false;
	float SkillSpikeElapsed = 0.0f;

	void UpdateElement(EHktDeconstructElement NewElement);
	void HandleDamage(float NewHealthRatio, float OldHealthRatio);
	void HandleDeath();
	void HandleSpawn();
	void HandleSkillActivate();
	void TickParamInterpolation(float DeltaTime);
};
