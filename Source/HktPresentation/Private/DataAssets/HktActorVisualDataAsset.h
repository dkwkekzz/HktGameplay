// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktTagDataAsset.h"
#include "HktActorVisualDataAsset.generated.h"

class AActor;
class UAnimMontage;
class UAnimSequence;
class UBlendSpace;

/**
 * 엔티티별 애니메이션 태그 → 에셋 매핑 (DataAsset 레벨)
 * AnimBP 기본값과 별개로, 엔티티 비주얼마다 다른 몽타주를 지정할 수 있습니다.
 * SpawnActor 시 AnimInstance에 동적 등록됩니다.
 */
USTRUCT(BlueprintType)
struct FHktVisualAnimMapping
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
 * 엔티티 시각화용 TagDataAsset.
 * IdentifierTag(예: Entity.Character.Player)로 로드되며, 스폰할 액터/블루프린트 클래스를 지정합니다.
 * AnimMappings를 통해 엔티티별 몽타주 매핑을 지정할 수 있습니다.
 */
UCLASS(BlueprintType)
class HKTPRESENTATION_API UHktActorVisualDataAsset : public UHktTagDataAsset
{
	GENERATED_BODY()

public:
	/** 이 시각 태그에 대응하는 액터 또는 블루프린트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Visual")
	TSubclassOf<AActor> ActorClass;

	/**
	 * 엔티티별 애니메이션 매핑 (Anim.Montage.Skill 등)
	 * 스폰 시 AnimInstance에 RegisterAnimMapping으로 등록됩니다.
	 * AnimBP 기본값보다 우선하여 덮어씁니다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TArray<FHktVisualAnimMapping> AnimMappings;
};
