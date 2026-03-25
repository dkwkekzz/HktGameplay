// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktStoryTypes.h"

/**
 * FHktStoryValidator — Story 바이트코드 정적 검증기
 *
 * Build() 시점에 컴파일된 바이트코드의 레지스터 흐름을 분석하여
 * 런타임 버그를 사전에 감지한다.
 *
 * 검증 항목:
 * 1. EntityFlow: 엔티티 레지스터(R10~R14) 초기화 순서 검증
 * 2. RegisterFlow: 범용 레지스터(R0~R8) Read-before-Write / Dead Write 감지
 *
 * 검증 정책:
 * - Editor: Warning 로그 출력, 게임 실행 허용 (무시 가능)
 * - Game(Shipping): EntityFlow 실패 시 등록 차단 + checkf
 */
class HKTCORE_API FHktStoryValidator
{
public:
	/**
	 * @param InCode      컴파일된 명령어 배열
	 * @param InTag       Story 태그 (로그 출력용)
	 * @param InLabelPCs  Label → PC 매핑 (합류점 판정용)
	 */
	FHktStoryValidator(
		const TArray<FInstruction>& InCode,
		const FGameplayTag& InTag,
		const TMap<FString, int32>& InLabels);

	/**
	 * 엔티티 레지스터(R10~R14) 초기화 순서 검증
	 * @return true면 검증 통과, false면 치명적 오류 (등록 차단)
	 */
	bool ValidateEntityFlow();

	/**
	 * 범용 레지스터(R0~R8) 흐름 검증
	 * - Read-before-Write: 초기화 안 된 레지스터 읽기
	 * - Dead Write: 값을 쓰고 읽지 않고 다시 덮어쓰기
	 *
	 * @return 경고 수 (0이면 이상 없음)
	 */
	int32 ValidateRegisterFlow();

private:
	const TArray<FInstruction>& Code;
	const FGameplayTag& Tag;
	TSet<int32> LabelPCs;
};
