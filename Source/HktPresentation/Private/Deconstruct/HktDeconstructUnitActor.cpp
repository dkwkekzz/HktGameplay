// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktDeconstructUnitActor.h"
#include "HktDeconstructParamController.h"
#include "Deconstruct/HktDeconstructVisualDataAsset.h"
#include "HktPresentationState.h"
#include "HktVFXIntent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"

namespace
{
	// 스킬 태그 감지용 접두사
	const FName SkeletalMeshBindingName = TEXT("SkeletalMeshComponent");
}

AHktDeconstructUnitActor::AHktDeconstructUnitActor()
{
	// Niagara Component 생성 (SkeletalMeshComponent에 부착)
	DeconstructNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DeconstructNiagara"));
	DeconstructNiagaraComponent->SetupAttachment(GetMeshComponent());
	DeconstructNiagaraComponent->SetAutoActivate(false);

	// Param Controller 컴포넌트
	ParamController = CreateDefaultSubobject<UHktDeconstructParamController>(TEXT("DeconstructParamController"));
}

void AHktDeconstructUnitActor::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint Class Defaults에서 설정된 DataAsset으로 자동 초기화
	if (DeconstructDataAsset && !bDeconstructInitialized)
	{
		InitializeDeconstruct(DeconstructDataAsset);
	}
}

void AHktDeconstructUnitActor::InitializeDeconstruct(const UHktDeconstructVisualDataAsset* InDataAsset)
{
	if (!InDataAsset || bDeconstructInitialized) return;
	CachedDataAsset = InDataAsset;
	bDeconstructInitialized = true;

	// Niagara System 설정
	if (InDataAsset->DeconstructSystem)
	{
		DeconstructNiagaraComponent->SetAsset(InDataAsset->DeconstructSystem);
	}

	// SkeletalMeshComponent를 Niagara에 바인딩 (Skeletal Mesh Location 모듈용)
	USkeletalMeshComponent* SkelMesh = GetMeshComponent();
	if (SkelMesh)
	{
		DeconstructNiagaraComponent->SetVariableObject(SkeletalMeshBindingName, SkelMesh);
	}

	// CoreGlow Material 적용 (E5: 기존 Material 제거, 반투명 발광으로 교체)
	if (InDataAsset->CoreGlowMaterial && SkelMesh)
	{
		const int32 NumMaterials = SkelMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			SkelMesh->SetMaterial(i, InDataAsset->CoreGlowMaterial);
		}
	}

	// ParamController 초기화
	ParamController->Initialize(DeconstructNiagaraComponent);

	// 기본 Element 설정
	UpdateElement(EHktDeconstructElement::Fire);

	// 스폰 연출 시작
	HandleSpawn();

	// Niagara 활성화
	DeconstructNiagaraComponent->Activate(true);
}

void AHktDeconstructUnitActor::UpdateElement(EHktDeconstructElement NewElement)
{
	if (CurrentElement == NewElement && bDeconstructInitialized) return;
	CurrentElement = NewElement;

	const UHktDeconstructVisualDataAsset* DA = CachedDataAsset.Get();
	FHktDeconstructPalette Palette = DA
		? DA->GetPalette(NewElement)
		: HktDeconstructDefaults::GetDefaultPalette(NewElement);

	UStaticMesh* FragMesh = DA ? DA->GetFragmentMesh(NewElement) : nullptr;

	// 팔레트를 현재/타겟 파라미터에 반영
	TargetParams.BaseColor = Palette.Primary;
	TargetParams.SecondaryColor = Palette.Secondary;
	TargetParams.AccentColor = Palette.Accent;
	CurrentParams.BaseColor = Palette.Primary;
	CurrentParams.SecondaryColor = Palette.Secondary;
	CurrentParams.AccentColor = Palette.Accent;

	// ParamController에 Element 설정 Push
	if (ParamController)
	{
		ParamController->SetElement(NewElement, Palette, FragMesh);
	}
}

void AHktDeconstructUnitActor::HandleDamage(float NewHealthRatio, float OldHealthRatio)
{
	const float Delta = OldHealthRatio - NewHealthRatio;
	if (Delta <= 0.0f) return;

	// Agitation 스파이크: 데미지 비율에 비례, 최대 1.0
	const float Spike = FMath::Clamp(Delta * 5.0f, 0.0f, 1.0f);
	TargetParams.Agitation = FMath::Max(TargetParams.Agitation, Spike);

	// Coherence = HealthRatio에 연동
	TargetParams.Coherence = NewHealthRatio;
	TargetParams.PointDensity = FMath::Lerp(0.3f, 1.0f, NewHealthRatio);
}

void AHktDeconstructUnitActor::HandleDeath()
{
	bDeathAnimating = true;
	DeathAnimElapsed = 0.0f;

	// Coherence → 0, PointScatter → 50 (Tick에서 보간)
	TargetParams.Coherence = 0.0f;
	TargetParams.PointScatter = 50.0f;
	TargetParams.PointDensity = 0.0f;

	// SurfaceAura 폭발적 방출
	TargetParams.AuraSpawnRateMult = 3.0f;
	TargetParams.AuraVelocityMult = 2.0f;

	// EnergyRibbon 소멸
	TargetParams.RibbonWidthMult = 0.0f;
}

void AHktDeconstructUnitActor::HandleSpawn()
{
	bSpawnAnimating = true;
	SpawnAnimElapsed = 0.0f;

	// 초기 상태: 완전 분해
	CurrentParams.Coherence = 0.0f;
	CurrentParams.PointScatter = 50.0f;
	CurrentParams.PointDensity = 0.3f;

	// 목표: 완전 조립
	TargetParams.Coherence = 1.0f;
	TargetParams.PointScatter = 0.0f;
	TargetParams.PointDensity = 1.0f;
	TargetParams.Agitation = 0.0f;
	TargetParams.RibbonWidthMult = 1.0f;
	TargetParams.RibbonEmissiveMult = 1.0f;
	TargetParams.AuraSpawnRateMult = 1.0f;
	TargetParams.AuraVelocityMult = 1.0f;
	TargetParams.FragmentScaleMult = 1.0f;
}

void AHktDeconstructUnitActor::HandleSkillActivate()
{
	bSkillSpiking = true;
	SkillSpikeElapsed = 0.0f;

	// 즉시 스파이크 (Tick에서 감쇠)
	CurrentParams.RibbonWidthMult = 3.0f;
	CurrentParams.RibbonEmissiveMult = 5.0f;
	CurrentParams.FragmentScaleMult = 2.0f;
	CurrentParams.AuraVelocityMult = 3.0f;
}

void AHktDeconstructUnitActor::ApplyPresentation(
	const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll,
	TFunctionRef<AActor*(FHktEntityId)> GetActorFunc)
{
	// 부모 처리 (Transform 보간, Animation 동기화)
	Super::ApplyPresentation(Entity, Frame, bForceAll, GetActorFunc);

	if (!bDeconstructInitialized) return;

	// HealthRatio 변화 감지 → 피격/사망 처리
	if (bForceAll || Entity.HealthRatio.IsDirty(Frame))
	{
		const float NewHealthRatio = Entity.HealthRatio.Get();

		if (NewHealthRatio <= 0.0f && PrevHealthRatio > 0.0f)
		{
			HandleDeath();
		}
		else if (NewHealthRatio < PrevHealthRatio)
		{
			HandleDamage(NewHealthRatio, PrevHealthRatio);
		}
		else if (bForceAll)
		{
			// 초기 동기화 시 현재 체력 상태 반영
			TargetParams.Coherence = NewHealthRatio;
			TargetParams.PointDensity = FMath::Lerp(0.3f, 1.0f, NewHealthRatio);
		}

		PrevHealthRatio = NewHealthRatio;
	}

	// VisualElement 변경 감지 → Element 전환
	if (bForceAll || Entity.VisualElement.IsDirty(Frame))
	{
		const FGameplayTag VisualTag = Entity.VisualElement.Get();
		if (VisualTag.IsValid())
		{
			// VisualElement 태그에서 EHktVFXElement 추출 후 Deconstruct Element로 변환
			// 태그 형식: "Element.Fire", "Element.Ice" 등
			// 간단한 이름 매칭으로 처리
			const FString TagName = VisualTag.ToString();
			EHktDeconstructElement NewElement = CurrentElement;

			if (TagName.Contains(TEXT("Fire")))        NewElement = EHktDeconstructElement::Fire;
			else if (TagName.Contains(TEXT("Ice")))     NewElement = EHktDeconstructElement::Ice;
			else if (TagName.Contains(TEXT("Lightning"))) NewElement = EHktDeconstructElement::Lightning;
			else if (TagName.Contains(TEXT("Dark")) || TagName.Contains(TEXT("Void")))
				NewElement = EHktDeconstructElement::Void;
			else if (TagName.Contains(TEXT("Nature")))  NewElement = EHktDeconstructElement::Nature;

			UpdateElement(NewElement);
		}
	}

	// Velocity → 기본 Agitation (이동 중 미세 동요)
	if (bForceAll || Entity.Velocity.IsDirty(Frame))
	{
		if (!bDeathAnimating)
		{
			const float Speed = Entity.Velocity.Get().Size();
			const float BaseAgitation = FMath::Clamp(Speed / 600.0f, 0.0f, 0.3f);
			// 피격 스파이크와 합산하지 않고, 기본값으로만 설정 (스파이크가 더 크면 유지)
			TargetParams.Agitation = FMath::Max(BaseAgitation, TargetParams.Agitation > 0.3f ? TargetParams.Agitation : BaseAgitation);
		}
	}

	// 스킬 발동 감지: PendingAnimTriggers 중 스킬 관련 태그
	for (const FGameplayTag& AnimTag : Entity.PendingAnimTriggers)
	{
		const FString TagStr = AnimTag.ToString();
		if (TagStr.Contains(TEXT("Skill")) || TagStr.Contains(TEXT("Attack")))
		{
			HandleSkillActivate();
			break;
		}
	}
}

void AHktDeconstructUnitActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDeconstructInitialized) return;

	TickParamInterpolation(DeltaTime);

	// Niagara에 현재 파라미터 Push
	if (ParamController)
	{
		ParamController->PushParams(CurrentParams);
	}
}

void AHktDeconstructUnitActor::TickParamInterpolation(float DeltaTime)
{
	// 스폰 연출 진행
	if (bSpawnAnimating)
	{
		SpawnAnimElapsed += DeltaTime;
		if (SpawnAnimElapsed >= SpawnAnimDuration)
		{
			bSpawnAnimating = false;
			// 조립 완료 후 잔여 진동 (0.5초)
			TargetParams.Agitation = 0.2f;
		}
	}

	// 사망 연출 진행
	if (bDeathAnimating)
	{
		DeathAnimElapsed += DeltaTime;
		if (DeathAnimElapsed >= DeathAnimDuration)
		{
			bDeathAnimating = false;
		}
	}

	// 스킬 스파이크 감쇠
	if (bSkillSpiking)
	{
		SkillSpikeElapsed += DeltaTime;
		if (SkillSpikeElapsed >= SkillSpikeDuration)
		{
			bSkillSpiking = false;
			// 스파이크 종료 → 기본값으로 복귀
			TargetParams.RibbonWidthMult = 1.0f;
			TargetParams.RibbonEmissiveMult = 1.0f;
			TargetParams.FragmentScaleMult = 1.0f;
			TargetParams.AuraVelocityMult = 1.0f;
		}
	}

	// 보간 속도 설정
	constexpr float CoherenceSpeed = 2.0f;     // 형태 변화는 느리게
	constexpr float AgitationSpeed = 4.0f;     // 동요는 빠르게 감쇠
	constexpr float ScatterSpeed = 2.0f;       // 분해/조립은 느리게
	constexpr float SpikeFalloff = 6.0f;       // 스킬 스파이크는 빠르게 감쇠

	// Coherence, PointScatter, PointDensity
	CurrentParams.Coherence = FMath::FInterpTo(CurrentParams.Coherence, TargetParams.Coherence, DeltaTime, CoherenceSpeed);
	CurrentParams.PointScatter = FMath::FInterpTo(CurrentParams.PointScatter, TargetParams.PointScatter, DeltaTime, ScatterSpeed);
	CurrentParams.PointDensity = FMath::FInterpTo(CurrentParams.PointDensity, TargetParams.PointDensity, DeltaTime, CoherenceSpeed);

	// Agitation 감쇠
	if (!bDeathAnimating && !bSkillSpiking)
	{
		// 전투 중이 아니면 Agitation을 기본 이동 수준으로 감쇠
		TargetParams.Agitation = FMath::FInterpTo(TargetParams.Agitation, 0.0f, DeltaTime, 2.0f);
	}
	CurrentParams.Agitation = FMath::FInterpTo(CurrentParams.Agitation, TargetParams.Agitation, DeltaTime, AgitationSpeed);

	// Ribbon/Fragment/Aura 배율 보간
	CurrentParams.RibbonWidthMult = FMath::FInterpTo(CurrentParams.RibbonWidthMult, TargetParams.RibbonWidthMult, DeltaTime, SpikeFalloff);
	CurrentParams.RibbonEmissiveMult = FMath::FInterpTo(CurrentParams.RibbonEmissiveMult, TargetParams.RibbonEmissiveMult, DeltaTime, SpikeFalloff);
	CurrentParams.AuraVelocityMult = FMath::FInterpTo(CurrentParams.AuraVelocityMult, TargetParams.AuraVelocityMult, DeltaTime, SpikeFalloff);
	CurrentParams.AuraSpawnRateMult = FMath::FInterpTo(CurrentParams.AuraSpawnRateMult, TargetParams.AuraSpawnRateMult, DeltaTime, SpikeFalloff);
	CurrentParams.FragmentScaleMult = FMath::FInterpTo(CurrentParams.FragmentScaleMult, TargetParams.FragmentScaleMult, DeltaTime, SpikeFalloff);
}
