// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"

FGameplayTag UHktAnimInstance::ExtractLayerParent(const FGameplayTag& AnimTag)
{
	// Anim.FullBody.Locomotion.Run → Anim.FullBody
	// Anim.UpperBody.Combat.Attack → Anim.UpperBody
	// 태그 이름에서 두 번째 레벨까지 추출
	FString TagStr = AnimTag.ToString();
	int32 FirstDot = INDEX_NONE;
	int32 SecondDot = INDEX_NONE;
	TagStr.FindChar(TEXT('.'), FirstDot);
	if (FirstDot != INDEX_NONE)
	{
		SecondDot = TagStr.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstDot + 1);
	}
	if (SecondDot != INDEX_NONE)
	{
		FString ParentStr = TagStr.Left(SecondDot);
		return FGameplayTag::RequestGameplayTag(FName(*ParentStr), false);
	}
	// 2레벨 이하의 태그는 그대로 반환
	return AnimTag;
}

void UHktAnimInstance::SetAnimTag(const FGameplayTag& AnimTag)
{
	FGameplayTag LayerParent = ExtractLayerParent(AnimTag);

	FGameplayTag& Current = AnimLayerTags.FindOrAdd(LayerParent);
	if (Current == AnimTag)
	{
		return;
	}

	Current = AnimTag;

	// FullBody는 AnimStateTag와 동기화 (하위호환)
	static const FGameplayTag FullBodyParent = FGameplayTag::RequestGameplayTag(FName(TEXT("Anim.FullBody")), false);
	if (LayerParent.MatchesTagExact(FullBodyParent))
	{
		AnimStateTag = AnimTag;
	}

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] SetAnimTag: Layer=%s Anim=%s on %s"),
		*LayerParent.ToString(), *AnimTag.ToString(), *GetOwningActor()->GetName());
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
