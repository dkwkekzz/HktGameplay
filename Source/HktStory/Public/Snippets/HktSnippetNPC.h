// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktStoryBuilder.h"
#include "GameplayTagContainer.h"

/**
 * HktSnippetNPC — NPC 스폰 관련 Story 패턴
 *
 * 모든 함수는 FHktStoryBuilder&를 받아 반환하여 fluent chaining을 지원한다.
 */
namespace HktSnippetNPC
{
	/** NPC 스탯 템플릿 */
	struct FHktNPCTemplate
	{
		int32 Health = 100;
		int32 AttackPower = 10;
		int32 Defense = 0;
		int32 MaxSpeed = 0;		// 0이면 설정 안 함
		int32 Team = 0;
	};

	/**
	 * Spawned 레지스터의 엔티티에 NPC 스탯 및 태그 설정
	 * IsNPC, Health, MaxHealth, AttackPower, Defense, (MaxSpeed), Team 설정
	 * Entity_NPC, SpecificTag, Tag_NPC_Hostile 태그 부여
	 *
	 * Clobbers: Temp (SaveConstEntity 내부)
	 * @param SpecificTag 예: Entity_NPC_Goblin
	 */
	HKTSTORY_API FHktStoryBuilder& SetupNPCStats(
		FHktStoryBuilder& B,
		const FGameplayTag& SpecificTag,
		const FHktNPCTemplate& Stats);

	/**
	 * 주기적 스포너 루프 시작부
	 * Label(LoopLabel) → HasPlayerInGroup → CountByTag → cap 초과 시 WaitLabel 점프
	 * 이 함수 이후에 스폰 로직을 배치한다.
	 *
	 * Clobbers: R0, R1, Flag
	 */
	HKTSTORY_API FHktStoryBuilder& SpawnerLoopBegin(
		FHktStoryBuilder& B,
		const FString& LoopLabel,
		const FString& WaitLabel,
		const FGameplayTag& CountTag,
		int32 Cap);

	/**
	 * 주기적 스포너 루프 종결부
	 * Label(WaitLabel) → WaitSeconds → Jump(LoopLabel)
	 */
	HKTSTORY_API FHktStoryBuilder& SpawnerLoopEnd(
		FHktStoryBuilder& B,
		const FString& LoopLabel,
		const FString& WaitLabel,
		float IntervalSeconds);
}
