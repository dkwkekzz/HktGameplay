// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktTagDataAsset.h"
#include "HktAnimInstance.h"
#include "HktActorVisualDataAsset.generated.h"

class AActor;

/**
 * 엔티티 시각화용 TagDataAsset.
 * IdentifierTag(예: Entity.Character.Player)로 로드되며, 스폰할 액터/블루프린트 클래스를 지정합니다.
 */
UCLASS(BlueprintType)
class HKTPRESENTATION_API UHktActorVisualDataAsset : public UHktTagDataAsset
{
	GENERATED_BODY()

public:
	/** 이 시각 태그에 대응하는 액터 또는 블루프린트 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Visual")
	TSubclassOf<AActor> ActorClass;

	/** 애니메이션 태그 → 몽타주 매핑 (스폰 시 AnimInstance에 주입) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TArray<FHktAnimMontageEntry> MontageMappings;

	/** 애니메이션 태그 → 시퀀스 매핑 (스폰 시 AnimInstance에 주입) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TArray<FHktAnimSequenceEntry> SequenceMappings;

	/** 애니메이션 태그 → 블렌드스페이스 매핑 (스폰 시 AnimInstance에 주입) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TArray<FHktAnimBlendSpaceEntry> BlendSpaceMappings;
};
