// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"

/**
 * HktSnippetItem — 아이템 관련 Story 패턴 (슬롯 디스패치, 스탯, 검증)
 *
 * 모든 함수는 FHktStoryBuilder&를 받아 반환하여 fluent chaining을 지원한다.
 */
namespace HktSnippetItem
{
	/**
	 * Param1(슬롯 인덱스) → ItemSlot[N] 로드 디스패치 테이블
	 * 슬롯 인덱스 무효 또는 아이템 0이면 FailLabel로 점프.
	 *
	 * Clobbers: R0, R1, DstReg, R3, Flag
	 */
	HKTSTORY_API FHktStoryBuilder& LoadItemFromSlot(
		FHktStoryBuilder& B,
		RegisterIndex DstReg,
		const FString& FailLabel);

	/**
	 * Self의 ItemSlot[SlotIndexReg]에 ValueReg 저장
	 *
	 * Clobbers: R4, Flag
	 */
	HKTSTORY_API FHktStoryBuilder& SaveItemToSlot(
		FHktStoryBuilder& B,
		RegisterIndex SlotIndexReg,
		RegisterIndex ValueReg);

	/**
	 * Self의 ItemSlot[SlotIndexReg] = 0 (클리어)
	 *
	 * Clobbers: R3, Flag
	 */
	HKTSTORY_API FHktStoryBuilder& ClearItemSlot(
		FHktStoryBuilder& B,
		RegisterIndex SlotIndexReg);

	/**
	 * 아이템 스탯(AttackPower, Defense)을 캐릭터에 합산
	 *
	 * Clobbers: R3, R4
	 */
	HKTSTORY_API FHktStoryBuilder& ApplyItemStats(
		FHktStoryBuilder& B,
		RegisterIndex ItemEntity,
		RegisterIndex CharEntity);

	/**
	 * 아이템 스탯(AttackPower, Defense)을 캐릭터에서 차감
	 *
	 * Clobbers: R0, R1
	 */
	HKTSTORY_API FHktStoryBuilder& RemoveItemStats(
		FHktStoryBuilder& B,
		RegisterIndex ItemEntity,
		RegisterIndex CharEntity);

	/**
	 * 소유자 검증: Entity의 OwnerEntity == Self
	 * 불일치 시 FailLabel로 점프.
	 *
	 * Clobbers: R0, Flag
	 */
	HKTSTORY_API FHktStoryBuilder& ValidateOwnership(
		FHktStoryBuilder& B,
		RegisterIndex Entity,
		const FString& FailLabel);

	/**
	 * 아이템 상태 검증: Entity의 ItemState == ExpectedState
	 * 불일치 시 FailLabel로 점프.
	 *
	 * Clobbers: R0, R1, Flag
	 * @param ExpectedState 0=Ground, 1=InBag, 2=Active
	 */
	HKTSTORY_API FHktStoryBuilder& ValidateItemState(
		FHktStoryBuilder& B,
		RegisterIndex Entity,
		int32 ExpectedState,
		const FString& FailLabel);
}
