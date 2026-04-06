// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktUnitActor.h"
#include "Deconstruct/HktDeconstructTypes.h"
#include "HktDeconstructUnitActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
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

	// IHktPresentableActor override
	virtual void ApplyPresentation(const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll,
		TFunctionRef<AActor*(FHktEntityId)> GetActorFunc) override;

	virtual void Tick(float DeltaTime) override;

	virtual void BeginPlay() override;

	/** Deconstruction DataAsset 설정. 스폰 후 초기화 시 호출. */
	void InitializeDeconstruct(const UHktDeconstructVisualDataAsset* InDataAsset);

protected:
	/**
	 * Deconstruction 비주얼 설정 DataAsset.
	 * Blueprint 서브클래스의 Class Defaults에서 지정한다.
	 * 런타임에 SpawnActor → BeginPlay 시 자동으로 InitializeDeconstruct() 호출.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Deconstruct")
	TObjectPtr<UHktDeconstructVisualDataAsset> DeconstructDataAsset;

	/** Deconstruction Niagara Component */
	UPROPERTY(VisibleAnywhere, Category = "HKT|Deconstruct")
	TObjectPtr<UNiagaraComponent> DeconstructNiagaraComponent;

private:
	/** Niagara 파라미터 브릿지 */
	UPROPERTY()
	TObjectPtr<UHktDeconstructParamController> ParamController;

	/** DataAsset 캐시 (팔레트/메시 참조용) */
	TWeakObjectPtr<const UHktDeconstructVisualDataAsset> CachedDataAsset;

	/** 현재 Element */
	EHktDeconstructElement CurrentElement = EHktDeconstructElement::Fire;

	/** 현재 보간 중인 파라미터 상태 */
	FHktDeconstructParams CurrentParams;

	/** 목표 파라미터 상태 (이벤트 발생 시 즉시 설정, Tick에서 CurrentParams로 보간) */
	FHktDeconstructParams TargetParams;

	/** 이전 프레임 HealthRatio (변화 감지용) */
	float PrevHealthRatio = 1.0f;

	/** 초기화 완료 여부 */
	bool bDeconstructInitialized = false;

	// --- 스폰 연출 ---
	bool bSpawnAnimating = false;
	float SpawnAnimElapsed = 0.0f;
	static constexpr float SpawnAnimDuration = 1.5f;

	// --- 사망 연출 ---
	bool bDeathAnimating = false;
	float DeathAnimElapsed = 0.0f;
	static constexpr float DeathAnimDuration = 2.0f;

	// --- 스킬 스파이크 ---
	bool bSkillSpiking = false;
	float SkillSpikeElapsed = 0.0f;
	static constexpr float SkillSpikeDuration = 0.5f;

	/** Element 변경 처리 */
	void UpdateElement(EHktDeconstructElement NewElement);

	/** 피격 처리: HealthRatio 감소 감지 → Agitation 스파이크 */
	void HandleDamage(float NewHealthRatio, float OldHealthRatio);

	/** 사망 처리: Coherence→0, PointScatter→50 Lerp 시작 */
	void HandleDeath();

	/** 스폰 처리: Coherence 0→1, PointScatter 50→0 Lerp 시작 */
	void HandleSpawn();

	/** 스킬 발동 처리: Ribbon/Fragment/Aura 스파이크 */
	void HandleSkillActivate();

	/** Tick 내 보간 업데이트 */
	void TickParamInterpolation(float DeltaTime);
};
