// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktDeconstructUnitActor.h"
#include "HktDeconstructParamController.h"
#include "Deconstruct/HktDeconstructVisualDataAsset.h"
#include "HktPresentationState.h"
#include "HktVFXIntent.h"
#include "NiagaraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Anim_Skill, "Anim.Skill");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Anim_Attack, "Anim.Attack");

// VisualElement 태그에서 Element 파싱용 (contains 방지)
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Element_Fire, "Element.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Element_Ice, "Element.Ice");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Element_Lightning, "Element.Lightning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Element_Dark, "Element.Dark");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Element_Void, "Element.Void");
UE_DEFINE_GAMEPLAY_TAG_STATIC(Tag_Element_Nature, "Element.Nature");

namespace
{
	EHktDeconstructElement ParseElementFromTag(const FGameplayTag& Tag)
	{
		if (Tag.MatchesTag(Tag_Element_Fire))       return EHktDeconstructElement::Fire;
		if (Tag.MatchesTag(Tag_Element_Ice))         return EHktDeconstructElement::Ice;
		if (Tag.MatchesTag(Tag_Element_Lightning))   return EHktDeconstructElement::Lightning;
		if (Tag.MatchesTag(Tag_Element_Dark) || Tag.MatchesTag(Tag_Element_Void))
			return EHktDeconstructElement::Void;
		if (Tag.MatchesTag(Tag_Element_Nature))      return EHktDeconstructElement::Nature;
		return EHktDeconstructElement::Fire;
	}
}

AHktDeconstructUnitActor::AHktDeconstructUnitActor()
{
	DeconstructNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DeconstructNiagara"));
	DeconstructNiagaraComponent->SetupAttachment(GetMeshComponent());
	DeconstructNiagaraComponent->SetAutoActivate(false);

	ParamController = CreateDefaultSubobject<UHktDeconstructParamController>(TEXT("DeconstructParamController"));
}

void AHktDeconstructUnitActor::BeginPlay()
{
	Super::BeginPlay();

	if (DeconstructDataAsset && !bDeconstructInitialized)
	{
		InitializeDeconstruct(DeconstructDataAsset);
	}
}

void AHktDeconstructUnitActor::InitializeDeconstruct(const UHktDeconstructVisualDataAsset* InDataAsset)
{
	if (!InDataAsset || bDeconstructInitialized) return;
	bDeconstructInitialized = true;

	// DeconstructDataAsset이 Blueprint에서 설정되지 않은 경우(외부 호출) 강제 설정
	if (!DeconstructDataAsset)
	{
		DeconstructDataAsset = const_cast<UHktDeconstructVisualDataAsset*>(InDataAsset);
	}

	if (InDataAsset->DeconstructSystem)
	{
		DeconstructNiagaraComponent->SetAsset(InDataAsset->DeconstructSystem);
	}

	// ParamController에 NiagaraComp + SkeletalMesh 바인딩 위임
	ParamController->Initialize(DeconstructNiagaraComponent, GetMeshComponent());

	// CoreGlow Material 적용 (E5)
	USkeletalMeshComponent* SkelMesh = GetMeshComponent();
	if (InDataAsset->CoreGlowMaterial && SkelMesh)
	{
		const int32 NumMaterials = SkelMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			SkelMesh->SetMaterial(i, InDataAsset->CoreGlowMaterial);
		}
	}

	UpdateElement(EHktDeconstructElement::Fire);
	HandleSpawn();
	DeconstructNiagaraComponent->Activate(true);
}

void AHktDeconstructUnitActor::UpdateElement(EHktDeconstructElement NewElement)
{
	if (CurrentElement == NewElement && bDeconstructInitialized) return;
	CurrentElement = NewElement;

	const UHktDeconstructVisualDataAsset* DA = DeconstructDataAsset.Get();
	FHktDeconstructPalette Palette = DA
		? DA->GetPalette(NewElement)
		: HktDeconstructDefaults::GetDefaultPalette(NewElement);

	UStaticMesh* FragMesh = DA ? DA->GetFragmentMesh(NewElement) : nullptr;

	// 색상은 즉시 전환 (보간 불필요)
	CurrentParams.BaseColor = Palette.Primary;
	CurrentParams.SecondaryColor = Palette.Secondary;
	CurrentParams.AccentColor = Palette.Accent;
	TargetParams.BaseColor = Palette.Primary;
	TargetParams.SecondaryColor = Palette.Secondary;
	TargetParams.AccentColor = Palette.Accent;

	ParamController->SetFragmentMesh(FragMesh);
	bParamsDirty = true;
}

void AHktDeconstructUnitActor::HandleDamage(float NewHealthRatio, float OldHealthRatio)
{
	const float Delta = OldHealthRatio - NewHealthRatio;
	if (Delta <= 0.0f) return;

	const float Spike = FMath::Clamp(Delta * DamageToAgitationScale, 0.0f, 1.0f);
	TargetParams.Agitation = FMath::Max(TargetParams.Agitation, Spike);
	TargetParams.Coherence = NewHealthRatio;
	TargetParams.PointDensity = FMath::Lerp(MinPointDensity, 1.0f, NewHealthRatio);
}

void AHktDeconstructUnitActor::HandleDeath()
{
	bDeathAnimating = true;
	DeathAnimElapsed = 0.0f;

	TargetParams.Coherence = 0.0f;
	TargetParams.PointScatter = MaxPointScatter;
	TargetParams.PointDensity = 0.0f;
	TargetParams.AuraSpawnRateMult = DeathAuraSpawnMult;
	TargetParams.AuraVelocityMult = DeathAuraVelMult;
	TargetParams.RibbonWidthMult = 0.0f;
}

void AHktDeconstructUnitActor::HandleSpawn()
{
	bSpawnAnimating = true;
	SpawnAnimElapsed = 0.0f;

	CurrentParams.Coherence = 0.0f;
	CurrentParams.PointScatter = MaxPointScatter;
	CurrentParams.PointDensity = 0.0f;

	TargetParams.Coherence = 1.0f;
	TargetParams.PointScatter = 0.0f;
	TargetParams.PointDensity = 1.0f;
	TargetParams.Agitation = 0.0f;
	TargetParams.RibbonWidthMult = 1.0f;
	TargetParams.RibbonEmissiveMult = 1.0f;
	TargetParams.AuraSpawnRateMult = 1.0f;
	TargetParams.AuraVelocityMult = 1.0f;
	TargetParams.FragmentScaleMult = 1.0f;
	bParamsDirty = true;
}

void AHktDeconstructUnitActor::HandleSkillActivate()
{
	bSkillSpiking = true;
	SkillSpikeElapsed = 0.0f;

	CurrentParams.RibbonWidthMult = SkillRibbonWidthMult;
	CurrentParams.RibbonEmissiveMult = SkillRibbonEmissiveMult;
	CurrentParams.FragmentScaleMult = SkillFragmentScaleMult;
	CurrentParams.AuraVelocityMult = SkillAuraVelMult;
	bParamsDirty = true;
}

void AHktDeconstructUnitActor::ApplyPresentation(
	const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll,
	TFunctionRef<AActor*(FHktEntityId)> GetActorFunc)
{
	Super::ApplyPresentation(Entity, Frame, bForceAll, GetActorFunc);

	if (!bDeconstructInitialized) return;

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
			TargetParams.Coherence = NewHealthRatio;
			TargetParams.PointDensity = FMath::Lerp(MinPointDensity, 1.0f, NewHealthRatio);
		}

		PrevHealthRatio = NewHealthRatio;
	}

	if (bForceAll || Entity.VisualElement.IsDirty(Frame))
	{
		const FGameplayTag VisualTag = Entity.VisualElement.Get();
		if (VisualTag.IsValid())
		{
			UpdateElement(ParseElementFromTag(VisualTag));
		}
	}

	if (bForceAll || Entity.Velocity.IsDirty(Frame))
	{
		if (!bDeathAnimating)
		{
			const float SpeedSq = Entity.Velocity.Get().SizeSquared();
			const float BaseAgitation = FMath::Clamp(
				FMath::Sqrt(SpeedSq) / MovementSpeedRef, 0.0f, MaxAgitationFromMovement);
			if (TargetParams.Agitation <= MaxAgitationFromMovement)
			{
				TargetParams.Agitation = BaseAgitation;
			}
		}
	}

	// 스킬/공격 감지: GameplayTag 매칭 (문자열 변환 없음)
	for (const FGameplayTag& AnimTag : Entity.PendingAnimTriggers)
	{
		if (AnimTag.MatchesTag(Tag_Anim_Skill) || AnimTag.MatchesTag(Tag_Anim_Attack))
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

	// 변경 감지: 이전에 Push한 값과 다를 때만 Niagara에 전달
	if (bParamsDirty || FMemory::Memcmp(&CurrentParams, &LastPushedParams, sizeof(FHktDeconstructParams)) != 0)
	{
		ParamController->PushParams(CurrentParams);
		LastPushedParams = CurrentParams;
		bParamsDirty = false;
	}
}

void AHktDeconstructUnitActor::TickParamInterpolation(float DeltaTime)
{
	if (bSpawnAnimating)
	{
		SpawnAnimElapsed += DeltaTime;
		if (SpawnAnimElapsed >= SpawnAnimDuration)
		{
			bSpawnAnimating = false;
			TargetParams.Agitation = PostSpawnAgitation;
		}
	}

	if (bDeathAnimating)
	{
		DeathAnimElapsed += DeltaTime;
		if (DeathAnimElapsed >= DeathAnimDuration)
		{
			bDeathAnimating = false;
		}
	}

	if (bSkillSpiking)
	{
		SkillSpikeElapsed += DeltaTime;
		if (SkillSpikeElapsed >= SkillSpikeDuration)
		{
			bSkillSpiking = false;
			TargetParams.RibbonWidthMult = 1.0f;
			TargetParams.RibbonEmissiveMult = 1.0f;
			TargetParams.FragmentScaleMult = 1.0f;
			TargetParams.AuraVelocityMult = 1.0f;
		}
	}

	constexpr float CoherenceSpeed = 2.0f;
	constexpr float AgitationSpeed = 4.0f;
	constexpr float ScatterSpeed = 2.0f;
	constexpr float SpikeFalloff = 6.0f;

	CurrentParams.Coherence = FMath::FInterpTo(CurrentParams.Coherence, TargetParams.Coherence, DeltaTime, CoherenceSpeed);
	CurrentParams.PointScatter = FMath::FInterpTo(CurrentParams.PointScatter, TargetParams.PointScatter, DeltaTime, ScatterSpeed);
	CurrentParams.PointDensity = FMath::FInterpTo(CurrentParams.PointDensity, TargetParams.PointDensity, DeltaTime, CoherenceSpeed);

	if (!bDeathAnimating && !bSkillSpiking)
	{
		TargetParams.Agitation = FMath::FInterpTo(TargetParams.Agitation, 0.0f, DeltaTime, 2.0f);
	}
	CurrentParams.Agitation = FMath::FInterpTo(CurrentParams.Agitation, TargetParams.Agitation, DeltaTime, AgitationSpeed);

	CurrentParams.RibbonWidthMult = FMath::FInterpTo(CurrentParams.RibbonWidthMult, TargetParams.RibbonWidthMult, DeltaTime, SpikeFalloff);
	CurrentParams.RibbonEmissiveMult = FMath::FInterpTo(CurrentParams.RibbonEmissiveMult, TargetParams.RibbonEmissiveMult, DeltaTime, SpikeFalloff);
	CurrentParams.AuraVelocityMult = FMath::FInterpTo(CurrentParams.AuraVelocityMult, TargetParams.AuraVelocityMult, DeltaTime, SpikeFalloff);
	CurrentParams.AuraSpawnRateMult = FMath::FInterpTo(CurrentParams.AuraSpawnRateMult, TargetParams.AuraSpawnRateMult, DeltaTime, SpikeFalloff);
	CurrentParams.FragmentScaleMult = FMath::FInterpTo(CurrentParams.FragmentScaleMult, TargetParams.FragmentScaleMult, DeltaTime, SpikeFalloff);
}
