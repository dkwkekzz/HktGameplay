// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimInstance.h"
#include "Animation/AnimMontage.h"

void UHktAnimInstance::SetAnimStateTag(const FGameplayTag& NewAnimTag)
{
	if (AnimStateTag == NewAnimTag)
	{
		return;
	}

	AnimStateTag = NewAnimTag;

	UE_LOG(LogTemp, Log, TEXT("[HktAnimInst] SetAnimStateTag: %s on %s"),
		*NewAnimTag.ToString(), *GetOwningActor()->GetName());
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

void UHktAnimInstance::InitMontageMappings(const TArray<FHktAnimMontageEntry>& InMappings)
{
	MontageMappings = InMappings;
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
