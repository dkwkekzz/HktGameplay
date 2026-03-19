// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"


namespace
{
	/** Anim.* 태그 필터 — Entity 태그 중 Anim 계열만 추출 */
	static const FGameplayTag AnimRootTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Anim")), false);
}

FGameplayTag UHktAnimInstance::ExtractLayerParent(const FGameplayTag& AnimTag)
{
	// Anim.FullBody.Locomotion.Run → Anim.FullBody
	// Anim.UpperBody.Combat.Attack → Anim.UpperBody
	// Anim.Montage.Attack → Anim.Montage
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
	return AnimTag;
}

void UHktAnimInstance::SyncFromTagContainer(const FGameplayTagContainer& EntityTags)
{
	// Entity 태그 중 Anim.* 계열만 필터링
	FGameplayTagContainer CurrentAnimTags = EntityTags.Filter(FGameplayTagContainer(AnimRootTag));

	// 새로 추가된 태그 감지 → 애니메이션 재생
	for (const FGameplayTag& Tag : CurrentAnimTags)
	{
		if (!PrevAnimTags.HasTagExact(Tag))
		{
			ApplyAnimTag(Tag);
		}
	}

	// 제거된 태그 감지 → 애니메이션 중지
	for (const FGameplayTag& Tag : PrevAnimTags)
	{
		if (!CurrentAnimTags.HasTagExact(Tag))
		{
			RemoveAnimTag(Tag);
		}
	}

	PrevAnimTags = CurrentAnimTags;
}

void UHktAnimInstance::ApplyAnimTag(const FGameplayTag& AnimTag)
{
	if (!AnimTag.IsValid())
	{
		return;
	}

	FGameplayTag LayerParent = ExtractLayerParent(AnimTag);

	FGameplayTag& Current = AnimLayerTags.FindOrAdd(LayerParent);
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

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] ApplyAnimTag: Parent=%s Anim=%s on %s"),
		*LayerParent.ToString(), *AnimTag.ToString(), *GetOwningActor()->GetName());
}

void UHktAnimInstance::RemoveAnimTag(const FGameplayTag& AnimTag)
{
	if (!AnimTag.IsValid())
	{
		return;
	}

	FGameplayTag LayerParent = ExtractLayerParent(AnimTag);

	// 해당 레이어의 현재 태그가 제거되는 태그와 일치하면 클리어
	if (FGameplayTag* Current = AnimLayerTags.Find(LayerParent))
	{
		if (Current->MatchesTagExact(AnimTag))
		{
			AnimLayerTags.Remove(LayerParent);
		}
	}

	// FullBody는 AnimStateTag와 동기화
	static const FGameplayTag FullBodyParent = FGameplayTag::RequestGameplayTag(FName(TEXT("Anim.FullBody")), false);
	if (LayerParent.MatchesTagExact(FullBodyParent) && AnimStateTag.MatchesTagExact(AnimTag))
	{
		AnimStateTag = FGameplayTag();
	}

	// 몽타주 계열 태그가 제거되면 몽타주 중지
	static const FGameplayTag MontageParent = FGameplayTag::RequestGameplayTag(FName(TEXT("Anim.Montage")), false);
	static const FGameplayTag UpperBodyParent = FGameplayTag::RequestGameplayTag(FName(TEXT("Anim.UpperBody")), false);
	if (LayerParent.MatchesTagExact(MontageParent) || LayerParent.MatchesTagExact(UpperBodyParent))
	{
		if (const FHktAnimMappingEntry* Entry = FindMapping(AnimTag))
		{
			if (Entry->Montage && IsPlayingMontageAnim())
			{
				Montage_Stop(0.25f, Entry->Montage);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] RemoveAnimTag: Parent=%s Anim=%s on %s"),
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

// ============================================================================
// 동적 매핑 등록 API
// ============================================================================

void UHktAnimInstance::RegisterAnimMapping(FGameplayTag AnimTag, UAnimMontage* Montage, UAnimSequence* Sequence, UBlendSpace* InBlendSpace)
{
	if (!AnimTag.IsValid()) return;

	// 기존 매핑이 있으면 덮어쓰기
	for (FHktAnimMappingEntry& Entry : AnimMappings)
	{
		if (Entry.AnimTag.MatchesTagExact(AnimTag))
		{
			Entry.Montage = Montage;
			Entry.Sequence = Sequence;
			Entry.BlendSpace = InBlendSpace;
			UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] Updated mapping: %s"), *AnimTag.ToString());
			return;
		}
	}

	// 새 매핑 추가
	FHktAnimMappingEntry NewEntry;
	NewEntry.AnimTag = AnimTag;
	NewEntry.Montage = Montage;
	NewEntry.Sequence = Sequence;
	NewEntry.BlendSpace = InBlendSpace;
	AnimMappings.Add(NewEntry);

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] Registered mapping: %s (Montage=%s, Sequence=%s, BlendSpace=%s)"),
		*AnimTag.ToString(),
		Montage ? *Montage->GetName() : TEXT("none"),
		Sequence ? *Sequence->GetName() : TEXT("none"),
		InBlendSpace ? *InBlendSpace->GetName() : TEXT("none"));
}

void UHktAnimInstance::UnregisterAnimMapping(FGameplayTag AnimTag)
{
	AnimMappings.RemoveAll([&AnimTag](const FHktAnimMappingEntry& Entry)
	{
		return Entry.AnimTag.MatchesTagExact(AnimTag);
	});
}

bool UHktAnimInstance::HasAnimMapping(FGameplayTag AnimTag) const
{
	return FindMapping(AnimTag) != nullptr;
}

// ============================================================================
// Stance — AnimBP 레이어 교체
// ============================================================================

void UHktAnimInstance::SyncStance(FGameplayTag NewStanceTag)
{
	if (StanceTag == NewStanceTag)
	{
		return;
	}

	FGameplayTag OldTag = StanceTag;
	StanceTag = NewStanceTag;

	// StanceAnimClassMap에서 새 Stance Tag에 해당하는 AnimClass 조회
	TSubclassOf<UAnimInstance>* FoundClass = StanceAnimClassMap.Find(NewStanceTag);
	TSubclassOf<UAnimInstance> NewStanceClass = FoundClass ? *FoundClass : nullptr;

	// 같은 클래스면 스킵
	if (NewStanceClass == CurrentLinkedStanceClass)
	{
		return;
	}

	// 기존 레이어 해제
	if (CurrentLinkedStanceClass)
	{
		UnlinkAnimClassLayers(CurrentLinkedStanceClass);
		UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] UnlinkStanceLayer: %s (Stance %s)"),
			*CurrentLinkedStanceClass->GetName(), *OldTag.ToString());
	}

	// 새 레이어 연결
	if (NewStanceClass)
	{
		LinkAnimClassLayers(NewStanceClass);
		UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] LinkStanceLayer: %s (Stance %s)"),
			*NewStanceClass->GetName(), *NewStanceTag.ToString());
	}

	CurrentLinkedStanceClass = NewStanceClass;
}
