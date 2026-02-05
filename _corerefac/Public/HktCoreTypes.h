// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

/**
 * HktCoreTypes.h - 호환 Shim (Compatibility Shim)
 *
 * 기존 코드의 #include "HktCoreTypes.h" 호환을 위해 유지됩니다.
 * 실제 타입 정의는 Common/ 서브디렉토리로 이동되었습니다.
 *
 * 새 코드에서는 직접 포함을 권장합니다:
 *   #include "Common/HktTypes.h"    // FHktEntityId, FHktVMHandle, Reg 등
 *   #include "Common/HktEvents.h"   // FHktIntentEvent, FHktSystemEvent 등
 */

#include "Common/HktTypes.h"
#include "Common/HktEvents.h"
