// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreDefs.h"

/**
 * 네임플레이트(HUD 위젯) 위의 커서 히트 테스트를 제공하는 인터페이스.
 * HUD에서 구현하여, SelectionPolicy가 3D 트레이스 실패 시 네임플레이트 클릭을 감지할 수 있게 합니다.
 */
class HKTRUNTIME_API IHktNameplateHitTestProvider
{
public:
	virtual ~IHktNameplateHitTestProvider() = default;

	/**
	 * 주어진 스크린 좌표가 엔티티 네임플레이트 위에 있는지 검사합니다.
	 * @param ScreenPos  뷰포트 픽셀 좌표
	 * @param OutEntityId  히트된 엔티티 ID
	 * @return 네임플레이트 히트 시 true
	 */
	virtual bool GetEntityUnderScreenPosition(const FVector2D& ScreenPos, FHktEntityId& OutEntityId) const = 0;
};
