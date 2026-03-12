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
 * 애니메이션 태그 → 몽타주 매핑 엔트리
 */
USTRUCT(BlueprintType)
struct FHktAnimMontageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	FGameplayTag AnimTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TObjectPtr<UAnimMontage> Montage;
};

/**
 * 애니메이션 태그 → 시퀀스 매핑 엔트리
 */
USTRUCT(BlueprintType)
struct FHktAnimSequenceEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	FGameplayTag AnimTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TObjectPtr<UAnimSequence> Sequence;
};

/**
 * 애니메이션 태그 → 블렌드스페이스 매핑 엔트리
 */
USTRUCT(BlueprintType)
struct FHktAnimBlendSpaceEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	FGameplayTag AnimTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|Animation")
	TObjectPtr<UBlendSpace> BlendSpace;
};

/**
 * UHktAnimInstance
 *
 * Flow VM의 AnimState/MontageState 프로퍼티를 UE5 애니메이션 시스템에 전달.
 * HktProperty를 통해 현재 재생 정보를 받고, 내부 매핑 테이블로 자체 재생.
 *
 * 태그 기반 레이어 시스템:
 * - 태그 계층으로 레이어 자동 감지: Anim.FullBody.* → FullBody, Anim.UpperBody.* → UpperBody
 * - AnimLayerTags: 레이어 부모 태그 → 현재 AnimTag 맵 (AnimBP에서 직접 읽기)
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

	/** 레이어별 애니메이션 상태 태그 (레이어 부모 태그 → AnimTag 매핑) */
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

	/** 애니메이션 태그 → 몽타주 매핑 */
	UPROPERTY(EditDefaultsOnly, Category = "HKT|Animation")
	TArray<FHktAnimMontageEntry> MontageMappings;

	/** 애니메이션 태그 → 시퀀스 매핑 */
	UPROPERTY(EditDefaultsOnly, Category = "HKT|Animation")
	TArray<FHktAnimSequenceEntry> SequenceMappings;

	/** 애니메이션 태그 → 블렌드스페이스 매핑 */
	UPROPERTY(EditDefaultsOnly, Category = "HKT|Animation")
	TArray<FHktAnimBlendSpaceEntry> BlendSpaceMappings;

	// ========== 제어 API ==========

	/** 애니메이션 태그 설정 — 태그 계층에서 레이어를 자동 감지하여 해당 레이어에 저장 */
	void SetAnimTag(const FGameplayTag& AnimTag);

	/** 특정 레이어의 애니메이션 상태 태그 조회 */
	UFUNCTION(BlueprintPure, Category = "HKT|Animation")
	FGameplayTag GetAnimLayerTag(const FGameplayTag& LayerTag) const;

	/** 몽타주 재생 (Anim.Montage.Attack 등) */
	void PlayMontageByTag(const FGameplayTag& MontageTag);

	/** 시퀀스 재생 — 슬롯 기반 다이나믹 몽타주로 재생 */
	void PlaySequenceByTag(const FGameplayTag& SequenceTag, FName SlotName = FName(TEXT("DefaultSlot")), float PlayRate = 1.0f);

	/** 블렌드스페이스 활성화 */
	void SetBlendSpaceByTag(const FGameplayTag& BlendSpaceTag);

	/** 몽타주가 재생 중인지 */
	UFUNCTION(BlueprintPure, Category = "HKT|Animation")
	bool IsPlayingMontageAnim() const;

private:
	static FGameplayTag ExtractLayerParent(const FGameplayTag& AnimTag);

	UAnimMontage* FindMontage(const FGameplayTag& Tag) const;
	UAnimSequence* FindSequence(const FGameplayTag& Tag) const;
	UBlendSpace* FindBlendSpace(const FGameplayTag& Tag) const;
};
