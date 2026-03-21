// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktTagDataAsset.h"
#include "GameplayTagContainer.h"
#include "HktAnimMontageDataAsset.generated.h"

class UAnimMontage;
class UAnimSequence;
class UBlendSpace;

/**
 * 애니메이션 태그 → 에셋 매핑 엔트리 (DataAsset 레벨).
 * 하나의 Anim 태그에 대해 Montage/Sequence/BlendSpace 중 설정된 에셋을 지정합니다.
 */
USTRUCT(BlueprintType)
struct FHktAnimMontageMapping
{
	GENERATED_BODY()

	/** 매핑할 애니메이션 태그 (예: Anim.Montage.Skill, Anim.Montage.Attack) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	FGameplayTag AnimTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TObjectPtr<UAnimSequence> Sequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TObjectPtr<UBlendSpace> BlendSpace;
};

/**
 * 엔티티별 애니메이션 몽타주 매핑 DataAsset.
 * IdentifierTag(예: Anim.Montage.Entity.Character.Player)로 검색하며,
 * 해당 엔티티가 사용할 Anim 태그 → 몽타주/시퀀스/블렌드스페이스 매핑을 보유합니다.
 *
 * UHktActorVisualDataAsset.AnimMontageTag로 참조되어
 * 스폰 시 AnimInstance에 RegisterAnimMapping으로 동적 등록됩니다.
 */
UCLASS(BlueprintType)
class HKTPRESENTATION_API UHktAnimMontageDataAsset : public UHktTagDataAsset
{
	GENERATED_BODY()

public:
	/** Anim 태그 → 몽타주/시퀀스/블렌드스페이스 매핑 테이블 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TArray<FHktAnimMontageMapping> AnimMappings;
};
