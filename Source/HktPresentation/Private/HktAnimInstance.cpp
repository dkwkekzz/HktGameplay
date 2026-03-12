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
	if (!AnimTag.IsValid())
	{
		return;
	}

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

	// 매핑 테이블에서 에셋을 찾아 자동 재생
	if (const FHktAnimMappingEntry* Entry = FindMapping(AnimTag))
	{
		if (Entry->Montage)
		{
			Montage_Play(Entry->Montage);
			UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] PlayMontage: %s -> %s"),
				*AnimTag.ToString(), *Entry->Montage->GetName());
		}
		else if (Entry->Sequence)
		{
			PlaySlotAnimationAsDynamicMontage(Entry->Sequence, FName(TEXT("DefaultSlot")), 0.25f, 0.25f, 1.0f);
			UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] PlaySequence: %s -> %s"),
				*AnimTag.ToString(), *Entry->Sequence->GetName());
		}

		if (Entry->BlendSpace && ActiveBlendSpace != Entry->BlendSpace)
		{
			ActiveBlendSpace = Entry->BlendSpace;
			UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] SetBlendSpace: %s -> %s"),
				*AnimTag.ToString(), *Entry->BlendSpace->GetName());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] SetAnimTag: Parent=%s Anim=%s on %s"),
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

bool UHktAnimInstance::IsPlayingMontageAnim() const
{
	return IsAnyMontagePlaying();
}

const FHktAnimMappingEntry* UHktAnimInstance::FindMapping(const FGameplayTag& Tag) const
{
	for (const FHktAnimMappingEntry& Entry : AnimMappings)
	{
		if (Entry.AnimTag.MatchesTagExact(Tag))
		{
			return &Entry;
		}
	}
	return nullptr;
}
