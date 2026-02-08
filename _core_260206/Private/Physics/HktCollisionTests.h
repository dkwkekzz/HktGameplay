// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktPhysicsTypes.h"
#include "HktPhysicsMath.h"

namespace HktPhysics
{
    // ========================================================================
    // Overlap 테스트 (bool 반환, 빠른 경로)
    // - 충돌 여부만 필요할 때 사용
    // - 상세 정보 불필요
    // ========================================================================
    
    /**
     * Sphere vs Sphere Overlap
     * @return 겹침 여부
     */
    FORCEINLINE bool OverlapSphereSphere(
        const FVector& CenterA, float RadiusA,
        const FVector& CenterB, float RadiusB)
    {
        const float RadiusSum = RadiusA + RadiusB;
        return DistanceSquared(CenterA, CenterB) <= RadiusSum * RadiusSum;
    }
    
    /**
     * Sphere vs Capsule Overlap
     * Capsule은 두 반구 + 원기둥으로 구성
     * @param SphereCenter - 구 중심
     * @param SphereRadius - 구 반경
     * @param CapsuleA - 캡슐 상단 중심점
     * @param CapsuleB - 캡슐 하단 중심점
     * @param CapsuleRadius - 캡슐 반경
     * @return 겹침 여부
     */
    FORCEINLINE bool OverlapSphereCapsule(
        const FVector& SphereCenter, float SphereRadius,
        const FVector& CapsuleA, const FVector& CapsuleB, float CapsuleRadius)
    {
        // 구 중심에서 캡슐 축(선분)까지의 최근접점
        const FVector ClosestOnCapsule = ClosestPointOnSegment(SphereCenter, CapsuleA, CapsuleB);
        
        // 최근접점과 구 중심 사이 거리 체크
        const float RadiusSum = SphereRadius + CapsuleRadius;
        return DistanceSquared(SphereCenter, ClosestOnCapsule) <= RadiusSum * RadiusSum;
    }
    
    /**
     * Capsule vs Capsule Overlap
     * 두 선분 사이 최근접 거리로 판정
     */
    FORCEINLINE bool OverlapCapsuleCapsule(
        const FVector& A1, const FVector& A2, float RadiusA,
        const FVector& B1, const FVector& B2, float RadiusB)
    {
        const float RadiusSum = RadiusA + RadiusB;
        return SegmentSegmentDistanceSquared(A1, A2, B1, B2) <= RadiusSum * RadiusSum;
    }
    
    // ========================================================================
    // 상세 충돌 테스트 (Contact Point, Normal, Depth 포함)
    // - 물리 응답이 필요할 때 사용
    // ========================================================================
    
    /**
     * Sphere vs Sphere 상세 테스트
     * @param OutContact - 접촉점 (두 구 사이 선 위)
     * @param OutNormal - A에서 B 방향 법선
     * @param OutDepth - 관통 깊이 (양수 = 겹침)
     * @return 충돌 여부
     */
    bool TestSphereSphere(
        const FVector& CenterA, float RadiusA,
        const FVector& CenterB, float RadiusB,
        FVector& OutContact, FVector& OutNormal, float& OutDepth);
    
    /**
     * Sphere vs Capsule 상세 테스트
     */
    bool TestSphereCapsule(
        const FVector& SphereCenter, float SphereRadius,
        const FVector& CapsuleA, const FVector& CapsuleB, float CapsuleRadius,
        FVector& OutContact, FVector& OutNormal, float& OutDepth);
    
    /**
     * Capsule vs Capsule 상세 테스트
     */
    bool TestCapsuleCapsule(
        const FVector& A1, const FVector& A2, float RadiusA,
        const FVector& B1, const FVector& B2, float RadiusB,
        FVector& OutContact, FVector& OutNormal, float& OutDepth);
    
    // ========================================================================
    // 레이캐스트
    // - 시선 체크, 발사체 경로 등에 사용
    // ========================================================================
    
    /**
     * Ray vs Sphere
     * @param Origin - 레이 시작점
     * @param Direction - 레이 방향 (정규화 안 해도 됨, 내부에서 정규화)
     * @param MaxDistance - 최대 거리
     * @param SphereCenter - 구 중심
     * @param SphereRadius - 구 반경
     * @param OutDistance - 히트 거리
     * @param OutPoint - 히트 위치
     * @param OutNormal - 히트 법선
     * @return 히트 여부
     */
    bool RaycastSphere(
        const FVector& Origin, const FVector& Direction, float MaxDistance,
        const FVector& SphereCenter, float SphereRadius,
        float& OutDistance, FVector& OutPoint, FVector& OutNormal);
    
    /**
     * Ray vs Capsule
     */
    bool RaycastCapsule(
        const FVector& Origin, const FVector& Direction, float MaxDistance,
        const FVector& CapsuleA, const FVector& CapsuleB, float CapsuleRadius,
        float& OutDistance, FVector& OutPoint, FVector& OutNormal);
    
    // ========================================================================
    // 스윕 테스트 (이동하는 구)
    // - 연속 충돌 감지에 사용
    // ========================================================================
    
    /**
     * Moving Sphere vs Static Sphere
     * @param Start - 이동 시작 위치
     * @param End - 이동 끝 위치
     * @param MovingRadius - 이동하는 구 반경
     * @param StaticCenter - 정지한 구 중심
     * @param StaticRadius - 정지한 구 반경
     * @param OutTime - 충돌 시점 (0~1, 1 = 충돌 없음)
     * @param OutContact - 충돌 지점
     * @param OutNormal - 충돌 법선
     * @return 충돌 여부
     */
    bool SweepSphereSphere(
        const FVector& Start, const FVector& End, float MovingRadius,
        const FVector& StaticCenter, float StaticRadius,
        float& OutTime, FVector& OutContact, FVector& OutNormal);
    
    /**
     * Moving Sphere vs Static Capsule
     */
    bool SweepSphereCapsule(
        const FVector& Start, const FVector& End, float MovingRadius,
        const FVector& CapsuleA, const FVector& CapsuleB, float CapsuleRadius,
        float& OutTime, FVector& OutContact, FVector& OutNormal);
    
    // ========================================================================
    // 범용 테스트 (타입 디스패치)
    // ========================================================================
    
    /**
     * 두 충돌체 Overlap 테스트 (타입 자동 판별)
     * @param TypeA - A 충돌체 타입
     * @param PosA - A 위치
     * @param RadiusA - A 반경
     * @param HalfHeightA - A 캡슐 절반 높이 (Sphere면 무시)
     * @param TypeB - B 충돌체 타입
     * @param PosB - B 위치  
     * @param RadiusB - B 반경
     * @param HalfHeightB - B 캡슐 절반 높이 (Sphere면 무시)
     * @return 겹침 여부
     */
    bool OverlapColliders(
        EHktColliderType TypeA, const FVector& PosA, float RadiusA, float HalfHeightA,
        EHktColliderType TypeB, const FVector& PosB, float RadiusB, float HalfHeightB);
    
    /**
     * 두 충돌체 상세 테스트 (타입 자동 판별)
     */
    bool TestColliders(
        EHktColliderType TypeA, const FVector& PosA, float RadiusA, float HalfHeightA,
        EHktColliderType TypeB, const FVector& PosB, float RadiusB, float HalfHeightB,
        FVector& OutContact, FVector& OutNormal, float& OutDepth);
}
