// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimInstance.h"
#include "HktRuntimeTags.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"

void UHktAnimInstance::SetAnimLayerTag(const FGameplayTag& LayerTag, const FGameplayTag& AnimTag)
{
	FGameplayTag& Current = AnimLayerTags.FindOrAdd(LayerTag);
	if (Current == AnimTag)
	{
		return;
	}

	Current = AnimTag;

	// FullBody 레이어는 AnimStateTag와 동기화 (하위호환)
	if (LayerTag.MatchesTagExact(HktGameplayTags::Anim_Layer_FullBody))
	{
		AnimStateTag = AnimTag;
	}

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] SetAnimLayerTag: Layer=%s Anim=%s on %s"),
		*LayerTag.ToString(), *AnimTag.ToString(), *GetOwningActor()->GetName());
}

void UHktAnimInstance::SetAnimStateTag(const FGameplayTag& NewAnimTag)
{
	SetAnimLayerTag(HktGameplayTags::Anim_Layer_FullBody, NewAnimTag);
}

FGameplayTag UHktAnimInstance::GetAnimLayerTag(const FGameplayTag& LayerTag) const
{
	if (const FGameplayTag* Found = AnimLayerTags.Find(LayerTag))
	{
		return *Found;
	}
	return FGameplayTag();
}

void UHktAnimInstance::PlayMontageByTag(const FGameplayTag& MontageTag)
{
	UAnimMontage* Montage = FindMontage(MontageTag);
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktAnimInst] PlayMontageByTag: No montage mapped for tag %s"), *MontageTag.ToString());
		return;
	}

	Montage_Play(Montage);

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] PlayMontageByTag: %s -> %s on %s"),
		*MontageTag.ToString(), *Montage->GetName(), *GetOwningActor()->GetName());
}

bool UHktAnimInstance::IsPlayingMontageAnim() const
{
	return IsAnyMontagePlaying();
}

void UHktAnimInstance::PlaySequenceByTag(const FGameplayTag& SequenceTag, FName SlotName, float PlayRate)
{
	UAnimSequence* Sequence = FindSequence(SequenceTag);
	if (!Sequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktAnimInst] PlaySequenceByTag: No sequence mapped for tag %s"), *SequenceTag.ToString());
		return;
	}

	PlaySlotAnimationAsDynamicMontage(Sequence, SlotName, 0.25f, 0.25f, PlayRate);

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] PlaySequenceByTag: %s -> %s on %s"),
		*SequenceTag.ToString(), *Sequence->GetName(), *GetOwningActor()->GetName());
}

void UHktAnimInstance::SetBlendSpaceByTag(const FGameplayTag& BlendSpaceTag)
{
	UBlendSpace* NewBS = FindBlendSpace(BlendSpaceTag);
	if (!NewBS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktAnimInst] SetBlendSpaceByTag: No blendspace mapped for tag %s"), *BlendSpaceTag.ToString());
		return;
	}

	if (ActiveBlendSpace == NewBS)
	{
		return;
	}

	ActiveBlendSpace = NewBS;

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] SetBlendSpaceByTag: %s -> %s on %s"),
		*BlendSpaceTag.ToString(), *NewBS->GetName(), *GetOwningActor()->GetName());
}

void UHktAnimInstance::InitMontageMappings(const TArray<FHktAnimMontageEntry>& InMappings)
{
	MontageMappings = InMappings;
}

void UHktAnimInstance::InitSequenceMappings(const TArray<FHktAnimSequenceEntry>& InMappings)
{
	SequenceMappings = InMappings;
}

void UHktAnimInstance::InitBlendSpaceMappings(const TArray<FHktAnimBlendSpaceEntry>& InMappings)
{
	BlendSpaceMappings = InMappings;
}

UAnimMontage* UHktAnimInstance::FindMontage(const FGameplayTag& Tag) const
{
	for (const FHktAnimMontageEntry& Entry : MontageMappings)
	{
		if (Entry.AnimTag.MatchesTagExact(Tag))
		{
			return Entry.Montage;
		}
	}
	return nullptr;
}

UAnimSequence* UHktAnimInstance::FindSequence(const FGameplayTag& Tag) const
{
	for (const FHktAnimSequenceEntry& Entry : SequenceMappings)
	{
		if (Entry.AnimTag.MatchesTagExact(Tag))
		{
			return Entry.Sequence;
		}
	}
	return nullptr;
}

UBlendSpace* UHktAnimInstance::FindBlendSpace(const FGameplayTag& Tag) const
{
	for (const FHktAnimBlendSpaceEntry& Entry : BlendSpaceMappings)
	{
		if (Entry.AnimTag.MatchesTagExact(Tag))
		{
			return Entry.BlendSpace;
		}
	}
	return nullptr;
}
