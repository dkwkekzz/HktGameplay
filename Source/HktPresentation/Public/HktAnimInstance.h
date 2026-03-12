// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "HktAnimInstance.generated.h"

class UAnimMontage;
class UAnimSequence;
class UBlendSpace;

/**
 * 애니메이션 태그 → 에셋 매핑 엔트리
 * 하나의 태그에 대해 Montage/Sequence/BlendSpace 중 설정된 에셋을 자동 재생
 */
USTRUCT(BlueprintType)
struct FHktAnimMappingEntry
{
	GENERATED_BODY()

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
 * UHktAnimInstance
 *
 * Flow VM의 AnimTag를 UE5 애니메이션 시스템에 전달.
 * SetAnimTag() 하나로 태그를 받으면 매핑 테이블에서 에셋을 찾아 자동 재생.
 *
 * 태그 계층 기반 애니메이션 관리:
 * - AnimTag에서 부모 태그를 추출하여 자동 분류: Anim.FullBody.* / Anim.UpperBody.*
 * - AnimLayerTags: 부모 태그 → 현재 AnimTag 맵 (AnimBP에서 직접 읽기)
 * - AnimStateTag: FullBody 편의 프로퍼티 (하위호환)
 *
 * 매핑 테이블은 AnimBP 클래스 기본값에서 직접 설정합니다.
 */
UCLASS()
class HKTPRESENTATION_API UHktAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// ========== 런타임 상태 (Renderer에서 갱신) ==========

	/** 부모 태그별 애니메이션 상태 (부모 태그 → AnimTag 매핑) */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	TMap<FGameplayTag, FGameplayTag> AnimLayerTags;

	/** FullBody 애니메이션 상태 태그 (하위호환) */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	FGameplayTag AnimStateTag;

	/** 이동 중 여부 */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	bool bIsMoving = false;

	/** 이동 속도 (cm/s) — VelX/VelY에서 계산 */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	float MoveSpeed = 0.0f;

	/** 현재 활성 블렌드스페이스 */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	TObjectPtr<UBlendSpace> ActiveBlendSpace;

	/** 블렌드스페이스 입력 파라미터 X (Speed 등) */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	float BlendSpaceX = 0.0f;

	/** 블렌드스페이스 입력 파라미터 Y (Direction 등) */
	UPROPERTY(BlueprintReadOnly, Category = "HKT|Animation")
	float BlendSpaceY = 0.0f;

	// ========== 매핑 테이블 (AnimBP 클래스 기본값에서 설정) ==========

	/** 애니메이션 태그 → 에셋 매핑 (Montage/Sequence/BlendSpace 중 설정된 것을 자동 재생) */
	UPROPERTY(EditDefaultsOnly, Category = "HKT|Animation")
	TArray<FHktAnimMappingEntry> AnimMappings;

	// ========== 제어 API ==========

	/** 애니메이션 태그 설정 — 매핑 테이블에서 에셋을 찾아 자동 재생 */
	void SetAnimTag(const FGameplayTag& AnimTag);

	/** 특정 부모 태그의 애니메이션 상태 태그 조회 */
	UFUNCTION(BlueprintPure, Category = "HKT|Animation")
	FGameplayTag GetAnimLayerTag(const FGameplayTag& LayerTag) const;

	/** 몽타주가 재생 중인지 */
	UFUNCTION(BlueprintPure, Category = "HKT|Animation")
	bool IsPlayingMontageAnim() const;

private:
	static FGameplayTag ExtractLayerParent(const FGameplayTag& AnimTag);

	const FHktAnimMappingEntry* FindMapping(const FGameplayTag& Tag) const;
};
