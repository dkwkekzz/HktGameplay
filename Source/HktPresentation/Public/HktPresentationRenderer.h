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

	/** 비동기 작업 완료 등으로 추가 Sync가 필요한지 여부 */
	virtual bool NeedsTick() const { return false; }

	/** 카메라 변경 시 재동기화가 필요한 렌더러인지 여부 (스크린 투영 등) */
	virtual bool NeedsCameraSync() const { return false; }
};
