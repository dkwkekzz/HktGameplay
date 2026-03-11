// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "HktAnimationComponent.generated.h"

class UAnimMontage;
class USkeletalMeshComponent;
class UAnimInstance;

/**
 * 애니메이션 태그 → 몽타주 매핑 엔트리
 */
USTRUCT(BlueprintType)
struct FHktAnimMontageEntry
{
	GENERATED_BODY()

	/** 이 엔트리가 반응할 GameplayTag (예: Anim.Montage.Attack) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	FGameplayTag AnimTag;

	/** 재생할 몽타주 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TObjectPtr<UAnimMontage> Montage;
};

/**
 * UHktAnimationComponent
 *
 * Flow의 OpCode(PlayAnim, PlayAnimMontage)로부터 전달된 GameplayTag를 받아
 * 실제 UE5 애니메이션을 재생하는 컴포넌트.
 *
 * - PlayAnim → SetAnimStateTag: 루프 애니메이션 상태를 AnimBlueprint에 전달
 * - PlayAnimMontage → PlayMontageByTag: 원샷 몽타주 재생
 *
 * 액터 블루프린트에 배치하고, MontageMappings에 태그→몽타주 매핑을 설정합니다.
 * 루프 애니메이션은 AnimBlueprint에서 AnimStateTag 값을 읽어 상태 머신을 구동합니다.
 */
UCLASS(ClassGroup=(HKT), meta=(BlueprintSpawnableComponent))
class HKTPRESENTATION_API UHktAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHktAnimationComponent();

	/** 루프 애니메이션 상태 태그 설정 (Anim.Idle, Anim.Run 등) */
	UFUNCTION(BlueprintCallable, Category = "HKT|Animation")
	void SetAnimStateTag(const FGameplayTag& NewAnimTag);

	/** 몽타주 재생 (Anim.Montage.Attack 등) */
	UFUNCTION(BlueprintCallable, Category = "HKT|Animation")
	void PlayMontageByTag(const FGameplayTag& MontageTag);

	/** 현재 루프 애니메이션 상태 태그 */
	UFUNCTION(BlueprintPure, Category = "HKT|Animation")
	FGameplayTag GetAnimStateTag() const { return CurrentAnimStateTag; }

	/** 몽타주가 재생 중인지 */
	UFUNCTION(BlueprintPure, Category = "HKT|Animation")
	bool IsPlayingMontage() const;

protected:
	/** 태그 → 몽타주 매핑 테이블 (에디터에서 설정) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TArray<FHktAnimMontageEntry> MontageMappings;

private:
	UAnimMontage* FindMontage(const FGameplayTag& Tag) const;
	UAnimInstance* GetAnimInstance() const;

	UPROPERTY()
	FGameplayTag CurrentAnimStateTag;
};
