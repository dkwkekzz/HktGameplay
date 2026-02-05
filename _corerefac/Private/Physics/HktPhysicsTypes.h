// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

/**
 * HktPhysicsTypes.h - 내부 호환 Shim
 *
 * 기존 Private 코드의 #include "Physics/HktPhysicsTypes.h" 호환을 위해 유지됩니다.
 * 실제 타입 정의는 Public/ 서브디렉토리로 이동되었습니다.
 *
 * 새 코드에서는 직접 포함을 권장합니다:
 *   #include "Physics/HktCollisionShapes.h"  // 충돌 타입, 결과 구조체, Layer
 *   #include "State/HktComponentTypes.h"     // PropertyId (Physics 포함)
 */

#include "Common/HktTypes.h"
#include "Physics/HktCollisionShapes.h"
#include "State/HktComponentTypes.h"
