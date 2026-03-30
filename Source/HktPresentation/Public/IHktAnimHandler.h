// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"

/**
 * 애니메이션 백엔드 추상 인터페이스.
 *
 * AnimRenderer가 태그 diff를 수행한 뒤, 실제 play/stop은 이 인터페이스를 통해
 * 백엔드에 위임한다.
 * - Actor 기반: UHktAnimInstance가 구현 (Montage/Sequence/BlendSpace)
 * - MassEntity 기반: 향후 구현 (VAT, ISM transform 등)
 */
class IHktAnimHandler
{
public:
	virtual ~IHktAnimHandler() = default;

	/** 애니메이션 태그 적용 — 매핑된 에셋 재생 */
	virtual void PlayAnimByTag(const FGameplayTag& AnimTag) = 0;

	/** 애니메이션 태그 제거 — 해당 에셋 중지 */
	virtual void StopAnimByTag(const FGameplayTag& AnimTag) = 0;
};
