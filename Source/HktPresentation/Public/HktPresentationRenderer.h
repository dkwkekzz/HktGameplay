// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktPresentationState.h"

/** State에서 이번 프레임 변경사항을 소비하여 렌더링에 반영 */
class IHktPresentationRenderer
{
public:
	virtual ~IHktPresentationRenderer() = default;
	virtual void Sync(const FHktPresentationState& State) = 0;
	virtual void Teardown() = 0;
};
