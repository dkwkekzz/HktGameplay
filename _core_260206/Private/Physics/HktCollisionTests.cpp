// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktCollisionTests.h"

namespace HktPhysics
{
    // ========================================================================
    // 상세 충돌 테스트 구현
    // ========================================================================
    
    bool TestSphereSphere(
        const FVector& CenterA, float RadiusA,
        const FVector& CenterB, float RadiusB,
        FVector& OutContact, FVector& OutNormal, float& OutDepth)
    {
        const FVector Delta = CenterB - CenterA;
        const float DistSq = LengthSquared(Delta);
        const float RadiusSum = RadiusA + RadiusB;
        
        if (DistSq > RadiusSum * RadiusSum)
        {
            return false;
        }
        
        const float Dist = FMath::Sqrt(DistSq);
        
        // 중심이 거의 같은 경우 (완전히 겹침)
        if (Dist < KINDA_SMALL_NUMBER)
        {
            OutNormal = FVector::UpVector;
            OutContact = CenterA;
            OutDepth = RadiusSum;
        }
        else
        {
            OutNormal = Delta / Dist;
            OutContact = CenterA + OutNormal * RadiusA;
            OutDepth = RadiusSum - Dist;
        }
        
        return true;
    }
    
    bool TestSphereCapsule(
        const FVector& SphereCenter, float SphereRadius,
        const FVector& CapsuleA, const FVector& CapsuleB, float CapsuleRadius,
        FVector& OutContact, FVector& OutNormal, float& OutDepth)
    {
        // 캡슐 축 위의 최근접점
        const FVector ClosestOnCapsule = ClosestPointOnSegment(SphereCenter, CapsuleA, CapsuleB);
        
        // 최근접점에서 구로의 테스트 (Sphere-Sphere와 동일)
        return TestSphereSphere(
            ClosestOnCapsule, CapsuleRadius,
            SphereCenter, SphereRadius,
            OutContact, OutNormal, OutDepth);
    }
    
    bool TestCapsuleCapsule(
        const FVector& A1, const FVector& A2, float RadiusA,
        const FVector& B1, const FVector& B2, float RadiusB,
        FVector& OutContact, FVector& OutNormal, float& OutDepth)
    {
        // 두 선분의 최근접점
        FVector ClosestA, ClosestB;
        const float DistSq = SegmentSegmentDistanceSquaredWithPoints(
            A1, A2, B1, B2, ClosestA, ClosestB);
        
        const float RadiusSum = RadiusA + RadiusB;
        
        if (DistSq > RadiusSum * RadiusSum)
        {
            return false;
        }
        
        // 최근접점 쌍에서 Sphere-Sphere 테스트
        return TestSphereSphere(
            ClosestA, RadiusA,
            ClosestB, RadiusB,
            OutContact, OutNormal, OutDepth);
    }
    
    // ========================================================================
    // 레이캐스트 구현
    // ========================================================================
    
    bool RaycastSphere(
        const FVector& Origin, const FVector& Direction, float MaxDistance,
        const FVector& SphereCenter, float SphereRadius,
        float& OutDistance, FVector& OutPoint, FVector& OutNormal)
    {
        // 방향 정규화
        const FVector Dir = SafeNormalize(Direction);
        if (Dir.IsNearlyZero())
        {
            return false;
        }
        
        const float T = RaySphereIntersection(Origin, Dir, SphereCenter, SphereRadius);
        
        if (T < 0.f || T > MaxDistance)
        {
            return false;
        }
        
        OutDistance = T;
        OutPoint = Origin + Dir * T;
        OutNormal = SafeNormalize(OutPoint - SphereCenter);
        
        // 레이가 구 내부에서 시작한 경우 법선 반전
        if (DistanceSquared(Origin, SphereCenter) < SphereRadius * SphereRadius)
        {
            OutNormal = -OutNormal;
        }
        
        return true;
    }
    
    bool RaycastCapsule(
        const FVector& Origin, const FVector& Direction, float MaxDistance,
        const FVector& CapsuleA, const FVector& CapsuleB, float CapsuleRadius,
        float& OutDistance, FVector& OutPoint, FVector& OutNormal)
    {
        const FVector Dir = SafeNormalize(Direction);
        if (Dir.IsNearlyZero())
        {
            return false;
        }
        
        float BestT = MaxDistance + 1.f;
        FVector BestPoint = FVector::ZeroVector;
        FVector BestNormal = FVector::ZeroVector;
        bool bHit = false;
        
        // 1. 상단 반구 테스트
        {
            float T;
            FVector Point, Normal;
            if (RaycastSphere(Origin, Dir, MaxDistance, CapsuleA, CapsuleRadius, T, Point, Normal))
            {
                // 반구 범위 체크 (캡슐 축 방향으로 양수인 쪽만)
                const FVector CapsuleDir = SafeNormalize(CapsuleB - CapsuleA);
                const FVector ToHit = Point - CapsuleA;
                if (FVector::DotProduct(ToHit, CapsuleDir) <= 0.f)
                {
                    if (T < BestT)
                    {
                        BestT = T;
                        BestPoint = Point;
                        BestNormal = Normal;
                        bHit = true;
                    }
                }
            }
        }
        
        // 2. 하단 반구 테스트
        {
            float T;
            FVector Point, Normal;
            if (RaycastSphere(Origin, Dir, MaxDistance, CapsuleB, CapsuleRadius, T, Point, Normal))
            {
                const FVector CapsuleDir = SafeNormalize(CapsuleA - CapsuleB);
                const FVector ToHit = Point - CapsuleB;
                if (FVector::DotProduct(ToHit, CapsuleDir) <= 0.f)
                {
                    if (T < BestT)
                    {
                        BestT = T;
                        BestPoint = Point;
                        BestNormal = Normal;
                        bHit = true;
                    }
                }
            }
        }
        
        // 3. 원기둥 측면 테스트
        {
            float T, AxisT;
            if (RayCylinderIntersection(Origin, Dir, CapsuleA, CapsuleB, CapsuleRadius, T, AxisT))
            {
                // AxisT가 0~1 범위 내여야 원기둥 부분
                if (AxisT >= 0.f && AxisT <= 1.f && T >= 0.f && T <= MaxDistance && T < BestT)
                {
                    BestT = T;
                    BestPoint = Origin + Dir * T;
                    
                    // 법선: 히트점에서 캡슐 축까지의 방향
                    const FVector AxisPoint = CapsuleA + (CapsuleB - CapsuleA) * AxisT;
                    BestNormal = SafeNormalize(BestPoint - AxisPoint);
                    bHit = true;
                }
            }
        }
        
        if (bHit)
        {
            OutDistance = BestT;
            OutPoint = BestPoint;
            OutNormal = BestNormal;
        }
        
        return bHit;
    }
    
    // ========================================================================
    // 스윕 테스트 구현
    // ========================================================================
    
    bool SweepSphereSphere(
        const FVector& Start, const FVector& End, float MovingRadius,
        const FVector& StaticCenter, float StaticRadius,
        float& OutTime, FVector& OutContact, FVector& OutNormal)
    {
        // 민코프스키 합: 정지 구의 반경을 확장
        const float CombinedRadius = MovingRadius + StaticRadius;
        
        // 이동 벡터
        const FVector Movement = End - Start;
        const float MovementLen = Movement.Size();
        
        if (MovementLen < KINDA_SMALL_NUMBER)
        {
            // 이동 없음 - 현재 위치에서 겹침 체크
            if (DistanceSquared(Start, StaticCenter) <= CombinedRadius * CombinedRadius)
            {
                OutTime = 0.f;
                OutNormal = SafeNormalize(Start - StaticCenter);
                if (OutNormal.IsNearlyZero())
                {
                    OutNormal = FVector::UpVector;
                }
                OutContact = StaticCenter + OutNormal * StaticRadius;
                return true;
            }
            return false;
        }
        
        // 레이-구 교차 (점을 확장된 구에 대해)
        const FVector Dir = Movement / MovementLen;
        const float T = RaySphereIntersection(Start, Dir, StaticCenter, CombinedRadius);
        
        if (T < 0.f || T > MovementLen)
        {
            return false;
        }
        
        OutTime = T / MovementLen;
        
        // 충돌 시점의 이동 구 중심
        const FVector HitCenter = Start + Dir * T;
        
        // 법선: 정지 구 중심에서 이동 구 중심 방향
        OutNormal = SafeNormalize(HitCenter - StaticCenter);
        if (OutNormal.IsNearlyZero())
        {
            OutNormal = -Dir;
        }
        
        // 접촉점: 정지 구 표면
        OutContact = StaticCenter + OutNormal * StaticRadius;
        
        return true;
    }
    
    bool SweepSphereCapsule(
        const FVector& Start, const FVector& End, float MovingRadius,
        const FVector& CapsuleA, const FVector& CapsuleB, float CapsuleRadius,
        float& OutTime, FVector& OutContact, FVector& OutNormal)
    {
        const float CombinedRadius = MovingRadius + CapsuleRadius;
        const FVector Movement = End - Start;
        const float MovementLen = Movement.Size();
        
        if (MovementLen < KINDA_SMALL_NUMBER)
        {
            // 정적 겹침 체크
            const FVector ClosestOnCapsule = ClosestPointOnSegment(Start, CapsuleA, CapsuleB);
            if (DistanceSquared(Start, ClosestOnCapsule) <= CombinedRadius * CombinedRadius)
            {
                OutTime = 0.f;
                OutNormal = SafeNormalize(Start - ClosestOnCapsule);
                if (OutNormal.IsNearlyZero())
                {
                    OutNormal = FVector::UpVector;
                }
                OutContact = ClosestOnCapsule + OutNormal * CapsuleRadius;
                return true;
            }
            return false;
        }
        
        const FVector Dir = Movement / MovementLen;
        float BestT = MovementLen + 1.f;
        FVector BestContact = FVector::ZeroVector;
        FVector BestNormal = FVector::ZeroVector;
        bool bHit = false;
        
        // 샘플링 기반 근사 (정확한 해석해는 복잡함)
        // 캡슐 축을 따라 여러 구에 대해 스윕
        constexpr int32 NumSamples = 8;
        const FVector CapsuleAxis = CapsuleB - CapsuleA;
        
        for (int32 i = 0; i <= NumSamples; ++i)
        {
            const float Alpha = static_cast<float>(i) / NumSamples;
            const FVector SampleCenter = CapsuleA + CapsuleAxis * Alpha;
            
            float T;
            FVector Contact, Normal;
            if (SweepSphereSphere(Start, End, MovingRadius, SampleCenter, CapsuleRadius, T, Contact, Normal))
            {
                const float AbsT = T * MovementLen;
                if (AbsT < BestT)
                {
                    BestT = AbsT;
                    BestContact = Contact;
                    BestNormal = Normal;
                    bHit = true;
                }
            }
        }
        
        if (bHit)
        {
            OutTime = BestT / MovementLen;
            OutContact = BestContact;
            OutNormal = BestNormal;
        }
        
        return bHit;
    }
    
    // ========================================================================
    // 범용 테스트 (타입 디스패치)
    // ========================================================================
    
    bool OverlapColliders(
        EHktColliderType TypeA, const FVector& PosA, float RadiusA, float HalfHeightA,
        EHktColliderType TypeB, const FVector& PosB, float RadiusB, float HalfHeightB)
    {
        // 캡슐 종단점 계산 헬퍼
        auto GetCapsuleEndpoints = [](const FVector& Center, float HalfHeight, FVector& OutA, FVector& OutB)
        {
            OutA = Center + FVector(0.f, 0.f, HalfHeight);
            OutB = Center - FVector(0.f, 0.f, HalfHeight);
        };
        
        // None 타입은 충돌 없음
        if (TypeA == EHktColliderType::None || TypeB == EHktColliderType::None)
        {
            return false;
        }
        
        // Sphere-Sphere
        if (TypeA == EHktColliderType::Sphere && TypeB == EHktColliderType::Sphere)
        {
            return OverlapSphereSphere(PosA, RadiusA, PosB, RadiusB);
        }
        
        // Sphere-Capsule
        if (TypeA == EHktColliderType::Sphere && TypeB == EHktColliderType::Capsule)
        {
            FVector CapsuleTop, CapsuleBottom;
            GetCapsuleEndpoints(PosB, HalfHeightB, CapsuleTop, CapsuleBottom);
            return OverlapSphereCapsule(PosA, RadiusA, CapsuleTop, CapsuleBottom, RadiusB);
        }
        
        // Capsule-Sphere (순서 반전)
        if (TypeA == EHktColliderType::Capsule && TypeB == EHktColliderType::Sphere)
        {
            FVector CapsuleTop, CapsuleBottom;
            GetCapsuleEndpoints(PosA, HalfHeightA, CapsuleTop, CapsuleBottom);
            return OverlapSphereCapsule(PosB, RadiusB, CapsuleTop, CapsuleBottom, RadiusA);
        }
        
        // Capsule-Capsule
        if (TypeA == EHktColliderType::Capsule && TypeB == EHktColliderType::Capsule)
        {
            FVector A1, A2, B1, B2;
            GetCapsuleEndpoints(PosA, HalfHeightA, A1, A2);
            GetCapsuleEndpoints(PosB, HalfHeightB, B1, B2);
            return OverlapCapsuleCapsule(A1, A2, RadiusA, B1, B2, RadiusB);
        }
        
        return false;
    }
    
    bool TestColliders(
        EHktColliderType TypeA, const FVector& PosA, float RadiusA, float HalfHeightA,
        EHktColliderType TypeB, const FVector& PosB, float RadiusB, float HalfHeightB,
        FVector& OutContact, FVector& OutNormal, float& OutDepth)
    {
        auto GetCapsuleEndpoints = [](const FVector& Center, float HalfHeight, FVector& OutA, FVector& OutB)
        {
            OutA = Center + FVector(0.f, 0.f, HalfHeight);
            OutB = Center - FVector(0.f, 0.f, HalfHeight);
        };
        
        if (TypeA == EHktColliderType::None || TypeB == EHktColliderType::None)
        {
            return false;
        }
        
        // Sphere-Sphere
        if (TypeA == EHktColliderType::Sphere && TypeB == EHktColliderType::Sphere)
        {
            return TestSphereSphere(PosA, RadiusA, PosB, RadiusB, OutContact, OutNormal, OutDepth);
        }
        
        // Sphere-Capsule
        if (TypeA == EHktColliderType::Sphere && TypeB == EHktColliderType::Capsule)
        {
            FVector CapsuleTop, CapsuleBottom;
            GetCapsuleEndpoints(PosB, HalfHeightB, CapsuleTop, CapsuleBottom);
            return TestSphereCapsule(PosA, RadiusA, CapsuleTop, CapsuleBottom, RadiusB,
                OutContact, OutNormal, OutDepth);
        }
        
        // Capsule-Sphere
        if (TypeA == EHktColliderType::Capsule && TypeB == EHktColliderType::Sphere)
        {
            FVector CapsuleTop, CapsuleBottom;
            GetCapsuleEndpoints(PosA, HalfHeightA, CapsuleTop, CapsuleBottom);
            if (TestSphereCapsule(PosB, RadiusB, CapsuleTop, CapsuleBottom, RadiusA,
                OutContact, OutNormal, OutDepth))
            {
                // 법선 방향 반전 (A → B)
                OutNormal = -OutNormal;
                return true;
            }
            return false;
        }
        
        // Capsule-Capsule
        if (TypeA == EHktColliderType::Capsule && TypeB == EHktColliderType::Capsule)
        {
            FVector A1, A2, B1, B2;
            GetCapsuleEndpoints(PosA, HalfHeightA, A1, A2);
            GetCapsuleEndpoints(PosB, HalfHeightB, B1, B2);
            return TestCapsuleCapsule(A1, A2, RadiusA, B1, B2, RadiusB,
                OutContact, OutNormal, OutDepth);
        }
        
        return false;
    }
}
