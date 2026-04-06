// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Deconstruct/HktDeconstructTypes.h"
#include "HktDeconstructParamController.generated.h"

class UNiagaraComponent;
class UStaticMesh;

/**
 * FHktDeconstructParams → Niagara User Parameter 일괄 전송을 담당하는 컴포넌트.
 *
 * AHktDeconstructUnitActor가 소유하며, 매 Tick에서 PushParams()를 호출하여
 * 보간된 파라미터 값을 Niagara System에 전달한다.
 *
 * Niagara User Parameter 이름은 NS_HktDeconstruct 에셋과의 계약이다.
 */
UCLASS()
class UHktDeconstructParamController : public UActorComponent
{
	GENERATED_BODY()

public:
	UHktDeconstructParamController();

	/** NiagaraComponent 바인딩 */
	void Initialize(UNiagaraComponent* InNiagaraComp);

	/** 현재 파라미터를 Niagara에 Push. 매 Tick 호출. */
	void PushParams(const FHktDeconstructParams& Params);

	/** Element 전환 시 팔레트 + Fragment 메시 변경 */
	void SetElement(EHktDeconstructElement Element,
	                const FHktDeconstructPalette& Palette,
	                UStaticMesh* FragmentMesh);

private:
	TWeakObjectPtr<UNiagaraComponent> NiagaraComp;

	// Niagara User Parameter 이름 캐시 (매 프레임 FName 생성 방지)
	static const FName PN_Coherence;
	static const FName PN_PointScatter;
	static const FName PN_PointDensity;
	static const FName PN_Agitation;
	static const FName PN_BaseColor;
	static const FName PN_SecondaryColor;
	static const FName PN_AccentColor;
	static const FName PN_PulseRate;
	static const FName PN_TrailLifetime;
	static const FName PN_RibbonWidthMult;
	static const FName PN_RibbonEmissiveMult;
	static const FName PN_AuraVelocityMult;
	static const FName PN_AuraSpawnRateMult;
	static const FName PN_FragmentScaleMult;
	static const FName PN_FragmentMesh;
};
