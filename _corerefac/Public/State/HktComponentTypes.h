// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * PropertyId - 엔티티 속성 ID 상수
 *
 * SOA 레이아웃에서 각 속성의 인덱스를 정의
 * WorldState, LocalContext, VM 등에서 공통 사용
 */
namespace PropertyId
{
    // === 위치/이동 (0~9) ===
    constexpr uint16 PosX = 0;
    constexpr uint16 PosY = 1;
    constexpr uint16 PosZ = 2;
    constexpr uint16 RotYaw = 3;

    constexpr uint16 MoveTargetX = 4;
    constexpr uint16 MoveTargetY = 5;
    constexpr uint16 MoveTargetZ = 6;
    constexpr uint16 MoveSpeed = 7;
    constexpr uint16 IsMoving = 8;

    // === 전투/상태 (10~19) ===
    constexpr uint16 Health = 10;
    constexpr uint16 MaxHealth = 11;
    constexpr uint16 AttackPower = 12;
    constexpr uint16 Defense = 13;
    constexpr uint16 Team = 14;

    constexpr uint16 Mana = 15;
    constexpr uint16 MaxMana = 16;

    // === 소유/타입 (20~29) ===
    constexpr uint16 OwnerEntity = 20;
    constexpr uint16 EntityType = 21;

    // === 이벤트 파라미터 (30~39) ===
    constexpr uint16 TargetPosX = 30;
    constexpr uint16 TargetPosY = 31;
    constexpr uint16 TargetPosZ = 32;
    constexpr uint16 Param0 = 33;
    constexpr uint16 Param1 = 34;
    constexpr uint16 Param2 = 35;
    constexpr uint16 Param3 = 36;

    // === 애니메이션/비주얼 (40~49) ===
    constexpr uint16 AnimState = 40;
    constexpr uint16 VisualState = 41;

    // === 소유권 (50~59) ===
    constexpr uint16 OwnerPlayerHash = 52;

    // === Physics (70~79) ===
    constexpr uint16 ColliderType = 70;       // EHktColliderType (None/Sphere/Capsule)
    constexpr uint16 ColliderRadius = 71;     // cm 단위 (int32)
    constexpr uint16 ColliderHalfHeight = 72; // Capsule 전용, cm 단위
    constexpr uint16 CollisionLayer = 73;     // 자신의 레이어 (uint8)
    constexpr uint16 CollisionMask = 74;      // 충돌 대상 마스크 (uint8)
}
