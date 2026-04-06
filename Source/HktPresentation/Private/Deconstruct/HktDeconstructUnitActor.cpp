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

	if (!DeconstructDataAsset)
	{
		DeconstructDataAsset = const_cast<UHktDeconstructVisualDataAsset*>(InDataAsset);
	}

	if (InDataAsset->DeconstructSystem)
	{
		DeconstructNiagaraComponent->SetAsset(InDataAsset->DeconstructSystem);
	}

	ParamController->Initialize(DeconstructNiagaraComponent, GetMeshComponent());

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

	// 스폰: 분해 상태에서 시작, Target은 조립 상태 → FInterpTo가 자연스럽게 조립
	CurrentParams.Coherence = 0.0f;
	CurrentParams.PointScatter = MaxPointScatter;
	CurrentParams.PointDensity = 0.0f;
	TargetParams.Coherence = 1.0f;
	TargetParams.PointScatter = 0.0f;
	TargetParams.PointDensity = 1.0f;
	bParamsDirty = true;

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

	CurrentParams.BaseColor = Palette.Primary;
	CurrentParams.SecondaryColor = Palette.Secondary;
	CurrentParams.AccentColor = Palette.Accent;
	TargetParams.BaseColor = Palette.Primary;
	TargetParams.SecondaryColor = Palette.Secondary;
	TargetParams.AccentColor = Palette.Accent;

	ParamController->SetFragmentMesh(FragMesh);
	bParamsDirty = true;
}

void AHktDeconstructUnitActor::ApplyPresentation(
	const FHktEntityPresentation& Entity, int64 Frame, bool bForceAll,
	TFunctionRef<AActor*(FHktEntityId)> GetActorFunc)
{
	Super::ApplyPresentation(Entity, Frame, bForceAll, GetActorFunc);

	if (!bDeconstructInitialized) return;

	// --- HealthRatio → Coherence, PointDensity, 사망 ---
	if (bForceAll || Entity.HealthRatio.IsDirty(Frame))
	{
		const float NewHealthRatio = Entity.HealthRatio.Get();

		if (NewHealthRatio <= 0.0f && !bDead)
		{
			// 사망: Target을 분해 상태로. FInterpTo가 2초 정도에 걸쳐 수렴.
			bDead = true;
			TargetParams.Coherence = 0.0f;
			TargetParams.PointScatter = MaxPointScatter;
			TargetParams.PointDensity = 0.0f;
			TargetParams.AuraSpawnRateMult = DeathAuraSpawnMult;
			TargetParams.AuraVelocityMult = DeathAuraVelMult;
			TargetParams.RibbonWidthMult = 0.0f;
		}
		else if (!bDead)
		{
			// 피격에 의한 Agitation 스파이크
			const float Delta = PrevHealthRatio - NewHealthRatio;
			if (Delta > 0.0f)
			{
				const float Spike = FMath::Clamp(Delta * DamageToAgitationScale, 0.0f, 1.0f);
				TargetParams.Agitation = FMath::Max(TargetParams.Agitation, Spike);
			}

			TargetParams.Coherence = NewHealthRatio;
			TargetParams.PointDensity = FMath::Lerp(MinPointDensity, 1.0f, NewHealthRatio);
		}

		PrevHealthRatio = NewHealthRatio;
	}

	// --- VisualElement → Element 전환 ---
	if (bForceAll || Entity.VisualElement.IsDirty(Frame))
	{
		const FGameplayTag VisualTag = Entity.VisualElement.Get();
		if (VisualTag.IsValid())
		{
			UpdateElement(ParseElementFromTag(VisualTag));
		}
	}

	// --- Velocity → Agitation 기본값 ---
	if (!bDead && (bForceAll || Entity.Velocity.IsDirty(Frame)))
	{
		const float Speed = FMath::Sqrt(Entity.Velocity.Get().SizeSquared());
		const float BaseAgitation = FMath::Clamp(Speed / MovementSpeedRef, 0.0f, MaxAgitationFromMovement);
		if (TargetParams.Agitation <= MaxAgitationFromMovement)
		{
			TargetParams.Agitation = BaseAgitation;
		}
	}

	// --- 스킬/공격 → Multiplier 스파이크 (CurrentParams에 직접, Target은 1.0 유지) ---
	for (const FGameplayTag& AnimTag : Entity.PendingAnimTriggers)
	{
		if (AnimTag.MatchesTag(Tag_Anim_Skill) || AnimTag.MatchesTag(Tag_Anim_Attack))
		{
			CurrentParams.RibbonWidthMult = SkillRibbonWidthMult;
			CurrentParams.RibbonEmissiveMult = SkillRibbonEmissiveMult;
			CurrentParams.FragmentScaleMult = SkillFragmentScaleMult;
			CurrentParams.AuraVelocityMult = SkillAuraVelMult;
			bParamsDirty = true;
			break;
		}
	}
}

void AHktDeconstructUnitActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDeconstructInitialized) return;

	// Agitation은 항상 0을 향해 자연 감쇠 (피격/이동이 다시 올리지 않는 한)
	TargetParams.Agitation = FMath::FInterpTo(TargetParams.Agitation, 0.0f, DeltaTime, InterpSpeed_AgitationDecay);

	// CurrentParams → TargetParams 보간
	CurrentParams.Coherence = FMath::FInterpTo(CurrentParams.Coherence, TargetParams.Coherence, DeltaTime, InterpSpeed_Coherence);
	CurrentParams.PointScatter = FMath::FInterpTo(CurrentParams.PointScatter, TargetParams.PointScatter, DeltaTime, InterpSpeed_Scatter);
	CurrentParams.PointDensity = FMath::FInterpTo(CurrentParams.PointDensity, TargetParams.PointDensity, DeltaTime, InterpSpeed_Coherence);
	CurrentParams.Agitation = FMath::FInterpTo(CurrentParams.Agitation, TargetParams.Agitation, DeltaTime, InterpSpeed_Agitation);
	CurrentParams.RibbonWidthMult = FMath::FInterpTo(CurrentParams.RibbonWidthMult, TargetParams.RibbonWidthMult, DeltaTime, InterpSpeed_Multipliers);
	CurrentParams.RibbonEmissiveMult = FMath::FInterpTo(CurrentParams.RibbonEmissiveMult, TargetParams.RibbonEmissiveMult, DeltaTime, InterpSpeed_Multipliers);
	CurrentParams.AuraVelocityMult = FMath::FInterpTo(CurrentParams.AuraVelocityMult, TargetParams.AuraVelocityMult, DeltaTime, InterpSpeed_Multipliers);
	CurrentParams.AuraSpawnRateMult = FMath::FInterpTo(CurrentParams.AuraSpawnRateMult, TargetParams.AuraSpawnRateMult, DeltaTime, InterpSpeed_Multipliers);
	CurrentParams.FragmentScaleMult = FMath::FInterpTo(CurrentParams.FragmentScaleMult, TargetParams.FragmentScaleMult, DeltaTime, InterpSpeed_Multipliers);

	// 변경 감지 후 Niagara에 전달
	if (bParamsDirty || FMemory::Memcmp(&CurrentParams, &LastPushedParams, sizeof(FHktDeconstructParams)) != 0)
	{
		ParamController->PushParams(CurrentParams);
		LastPushedParams = CurrentParams;
		bParamsDirty = false;
	}
}
