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
#include "Physics/HktPhysicsTypes.h"
