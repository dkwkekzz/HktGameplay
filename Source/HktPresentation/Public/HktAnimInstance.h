// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "HktAnimInstance.generated.h"

class UAnimMontage;

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
 * UHktAnimInstance
 *
 * Flow VM의 AnimState/MontageState 프로퍼티를 UE5 애니메이션 시스템에 전달.
 *
 * - AnimStateTag: 루프 애니메이션 상태 (AnimBP 상태머신에서 직접 읽음)
 * - bIsMoving: 이동 여부 (블렌드스페이스용)
 * - PlayMontageByTag(): 원샷 몽타주 재생
 *
 * 몽타주 매핑은 UHktActorVisualDataAsset에서 로딩하여 InitMontageMappings()로 주입합니다.
 */
UCLASS()
class HKTPRESENTATION_API UHktAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** 루프 애니메이션 상태 태그 (Anim.Idle, Anim.Run 등) — AnimBP에서 직접 읽기 */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	FGameplayTag AnimStateTag;

	/** 이동 중 여부 — 블렌드스페이스, 상태 전환 조건 등에 활용 */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	bool bIsMoving = false;

	/** 루프 애니메이션 상태 태그 설정 */
	void SetAnimStateTag(const FGameplayTag& NewAnimTag);

	/** 몽타주 재생 (Anim.Montage.Attack 등) */
	void PlayMontageByTag(const FGameplayTag& MontageTag);

	/** 몽타주가 재생 중인지 */
	UFUNCTION(BlueprintPure, Category = "HKT|Animation")
	bool IsPlayingMontageAnim() const;

	/** DataAsset에서 로딩한 매핑 테이블 주입 */
	void InitMontageMappings(const TArray<FHktAnimMontageEntry>& InMappings);

private:
	UAnimMontage* FindMontage(const FGameplayTag& Tag) const;

	UPROPERTY()
	TArray<FHktAnimMontageEntry> MontageMappings;
};
