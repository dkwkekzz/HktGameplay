// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

/**
 * HktVMTypes.h - 내부 호환 Shim
 *
 * 기존 Private 코드의 #include "HktVMTypes.h" 호환을 위해 유지됩니다.
 * 실제 타입 정의는 Public/ 서브디렉토리로 이동되었습니다.
 *
 * 새 코드에서는 직접 포함을 권장합니다:
 *   #include "Common/HktTypes.h"       // FHktEntityId, FHktVMHandle, Reg
 *   #include "VM/HktInstruction.h"     // EOpCode, FInstruction, EVMStatus
 *   #include "State/HktComponentTypes.h" // PropertyId
 */

#include "Common/HktTypes.h"
#include "Common/HktEvents.h"
#include "VM/HktInstruction.h"
#include "State/HktComponentTypes.h"
