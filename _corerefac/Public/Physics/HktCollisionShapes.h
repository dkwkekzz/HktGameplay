// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/HktTypes.h"

// ============================================================================
// 상수
// ============================================================================

namespace HktPhysics
{
    /** 최대 충돌체 수 (MaxEntities와 동일) */
    constexpr int32 MaxColliders = HktCore::MaxEntities;

    /** 충돌 감지 시 최대 결과 수 */
    constexpr int32 MaxOverlapResults = 64;

    /** 충돌 레이어 비트 플래그 */
    namespace Layer
    {
        constexpr uint8 None       = 0;
        constexpr uint8 Default    = 1 << 0;  // 0x01
        constexpr uint8 Player     = 1 << 1;  // 0x02
        constexpr uint8 Enemy      = 1 << 2;  // 0x04
        constexpr uint8 Projectile = 1 << 3;  // 0x08
        constexpr uint8 Trigger    = 1 << 4;  // 0x10
        constexpr uint8 Environment= 1 << 5;  // 0x20
        constexpr uint8 All        = 0xFF;

        // 일반적인 조합
        constexpr uint8 Characters = Player | Enemy;
        constexpr uint8 Damageable = Player | Enemy | Environment;
    }
}

// ============================================================================
// 충돌체 타입
// ============================================================================

enum class EHktColliderType : uint8
{
    None = 0,
    Sphere,
    Capsule,
    Max
};

// ============================================================================
// 충돌 결과 구조체
// ============================================================================

/** 충돌 쌍 (간단한 결과) */
struct FHktCollisionPair
{
    FHktEntityId EntityA = InvalidEntityId;
    FHktEntityId EntityB = InvalidEntityId;

    bool IsValid() const { return EntityA != InvalidEntityId && EntityB != InvalidEntityId; }
};

/** 상세 충돌 결과 */
struct FHktCollisionResult
{
    FHktEntityId EntityA = InvalidEntityId;
    FHktEntityId EntityB = InvalidEntityId;
    FVector ContactPoint = FVector::ZeroVector;
    FVector ContactNormal = FVector::ZeroVector;  // A → B 방향
    float PenetrationDepth = 0.f;

    bool IsValid() const { return EntityA != InvalidEntityId && EntityB != InvalidEntityId; }

    void Reset()
    {
        EntityA = InvalidEntityId;
        EntityB = InvalidEntityId;
        ContactPoint = FVector::ZeroVector;
        ContactNormal = FVector::ZeroVector;
        PenetrationDepth = 0.f;
    }
};

/** 레이캐스트 결과 */
struct FHktRaycastResult
{
    FHktEntityId HitEntity = InvalidEntityId;
    FVector HitPoint = FVector::ZeroVector;
    FVector HitNormal = FVector::ZeroVector;
    float Distance = FLT_MAX;

    bool IsValid() const { return HitEntity != InvalidEntityId && Distance < FLT_MAX; }

    void Reset()
    {
        HitEntity = InvalidEntityId;
        HitPoint = FVector::ZeroVector;
        HitNormal = FVector::ZeroVector;
        Distance = FLT_MAX;
    }
};

/** 스윕 테스트 결과 */
struct FHktSweepResult : public FHktRaycastResult
{
    float HitTime = 1.f;  // 0~1, 이동 거리 비율 (1 = 충돌 없음)

    void Reset()
    {
        FHktRaycastResult::Reset();
        HitTime = 1.f;
    }
};
