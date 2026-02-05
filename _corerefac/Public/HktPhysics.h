// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

/**
 * HktPhysics.h - 호환 Shim (Compatibility Shim)
 *
 * 기존 코드의 #include "HktPhysics.h" 호환을 위해 유지됩니다.
 * 실제 타입 정의는 Physics/ 서브디렉토리로 이동되었습니다.
 *
 * 새 코드에서는 직접 포함을 권장합니다:
 *   #include "Physics/HktCollisionShapes.h"  // 충돌 타입/형상
 *   #include "Physics/HktSpatialSystem.h"    // 통합 공간 관리
 */

#include "Physics/HktCollisionShapes.h"
#include "Physics/HktSpatialSystem.h"

// 레거시 호환: 기존 Private 헤더도 포함 (아직 제거되지 않은 경우)
// Phase 2에서 기존 HktPhysicsWorld 제거 후 이 include도 제거 예정
#include "Physics/HktPhysicsTypes.h"
#include "Physics/HktPhysicsWorld.h"

// Forward declarations
class IHktStashInterface;

/**
 * PhysicsWorld 팩토리 함수 (레거시 호환)
 *
 * 새 코드에서는 FHktSpatialSystem을 직접 사용하세요.
 * SimulationWorld가 SpatialSystem을 소유하고 관리합니다.
 */
HKTCORE_API TUniquePtr<FHktPhysicsWorld> CreatePhysicsWorld(IHktStashInterface* InStash);
