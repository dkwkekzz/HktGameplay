// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktAnimationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UHktAnimationComponent::UHktAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHktAnimationComponent::SetAnimStateTag(const FGameplayTag& NewAnimTag)
{
	if (CurrentAnimStateTag == NewAnimTag)
	{
		return;
	}

	CurrentAnimStateTag = NewAnimTag;

	UE_LOG(LogTemp, Log, TEXT("[HktAnimComp] SetAnimStateTag: %s on %s"),
		*NewAnimTag.ToString(), *GetOwner()->GetName());
}

void UHktAnimationComponent::PlayMontageByTag(const FGameplayTag& MontageTag)
{
	UAnimMontage* Montage = FindMontage(MontageTag);
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktAnimComp] PlayMontageByTag: No montage mapped for tag %s"), *MontageTag.ToString());
		return;
	}

	UAnimInstance* AnimInst = GetAnimInstance();
	if (!AnimInst)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HktAnimComp] PlayMontageByTag: No AnimInstance on %s"), *GetOwner()->GetName());
		return;
	}

	AnimInst->Montage_Play(Montage);

	UE_LOG(LogTemp, Log, TEXT("[HktAnimComp] PlayMontageByTag: %s -> %s on %s"),
		*MontageTag.ToString(), *Montage->GetName(), *GetOwner()->GetName());
}

bool UHktAnimationComponent::IsPlayingMontage() const
{
	UAnimInstance* AnimInst = GetAnimInstance();
	return AnimInst && AnimInst->IsAnyMontagePlaying();
}

UAnimMontage* UHktAnimationComponent::FindMontage(const FGameplayTag& Tag) const
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

UAnimInstance* UHktAnimationComponent::GetAnimInstance() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		return nullptr;
	}

	return SkelMesh->GetAnimInstance();
}
