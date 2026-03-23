// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"

/**
 * HktSnippetCombat — 전투 관련 Story 패턴 (쿨타임, 자원 회복)
 *
 * 모든 함수는 FHktStoryBuilder&를 받아 반환하여 fluent chaining을 지원한다.
 */
namespace HktSnippetCombat
{
	/**
	 * 쿨타임 검증 (공속 기반)
	 * GetWorldTime → NextActionFrame 비교 → FailLabel 점프
	 *
	 * Clobbers: R0, R1, Flag
	 */
	HKTSTORY_API FHktStoryBuilder& CooldownCheck(
		FHktStoryBuilder& B,
		const FString& FailLabel);

	/**
	 * 쿨타임 갱신 (상수 RecoveryFrame)
	 * NextActionFrame = CurrentFrame + (RecoveryFrame * 100 / AttackSpeed)
	 *
	 * Clobbers: R0, R1, R2
	 */
	HKTSTORY_API FHktStoryBuilder& CooldownUpdateConst(
		FHktStoryBuilder& B,
		int32 RecoveryFrame);

	/**
	 * 쿨타임 갱신 (엔티티에서 RecoveryFrame 읽기)
	 * NextActionFrame = CurrentFrame + (ItemEntity.RecoveryFrame * 100 / AttackSpeed)
	 *
	 * Clobbers: R0, R1, R3
	 * @param ItemEntity 레지스터 — RecoveryFrame을 읽을 아이템 엔티티
	 */
	HKTSTORY_API FHktStoryBuilder& CooldownUpdateFromEntity(
		FHktStoryBuilder& B,
		RegisterIndex ItemEntity);

	/**
	 * 자원 회복 + Max 클램프
	 * CurrentProp += Amount, MaxProp 초과 시 클램프
	 *
	 * Clobbers: R0, R1, R2, R3
	 */
	HKTSTORY_API FHktStoryBuilder& ResourceGainClamped(
		FHktStoryBuilder& B,
		uint16 CurrentProp,
		uint16 MaxProp,
		int32 Amount);
}
