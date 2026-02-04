// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktPhysicsMath.h"

namespace HktPhysics
{
    void ClosestPointsOnSegments(
        const FVector& A1, const FVector& A2,
        const FVector& B1, const FVector& B2,
        float& OutTA, float& OutTB)
    {
        const FVector D1 = A2 - A1;  // 선분 A 방향
        const FVector D2 = B2 - B1;  // 선분 B 방향
        const FVector R = A1 - B1;
        
        const float A = LengthSquared(D1);  // D1 · D1
        const float E = LengthSquared(D2);  // D2 · D2
        const float F = FVector::DotProduct(D2, R);
        
        // 두 선분 모두 점으로 퇴화한 경우
        if (A < KINDA_SMALL_NUMBER && E < KINDA_SMALL_NUMBER)
        {
            OutTA = 0.f;
            OutTB = 0.f;
            return;
        }
        
        // 선분 A가 점으로 퇴화
        if (A < KINDA_SMALL_NUMBER)
        {
            OutTA = 0.f;
            OutTB = FMath::Clamp(F / E, 0.f, 1.f);
            return;
        }
        
        const float C = FVector::DotProduct(D1, R);
        
        // 선분 B가 점으로 퇴화
        if (E < KINDA_SMALL_NUMBER)
        {
            OutTB = 0.f;
            OutTA = FMath::Clamp(-C / A, 0.f, 1.f);
            return;
        }
        
        // 일반 케이스
        const float B = FVector::DotProduct(D1, D2);
        const float Denom = A * E - B * B;
        
        // 평행한 경우
        if (FMath::Abs(Denom) < KINDA_SMALL_NUMBER)
        {
            OutTA = 0.f;
            OutTB = FMath::Clamp(F / E, 0.f, 1.f);
            
            // A의 시작점에서 B까지의 거리 vs A의 끝점에서 B까지의 거리 비교
            const FVector ClosestOnB = B1 + D2 * OutTB;
            const float DistSqStart = DistanceSquared(A1, ClosestOnB);
            const float DistSqEnd = DistanceSquared(A2, ClosestOnB);
            
            if (DistSqEnd < DistSqStart)
            {
                OutTA = 1.f;
                OutTB = FMath::Clamp(FVector::DotProduct(D2, A2 - B1) / E, 0.f, 1.f);
            }
            return;
        }
        
        // 무한 직선 상의 최근접점 파라미터
        float S = (B * F - C * E) / Denom;
        float T = (B * S + F) / E;
        
        // S 클램핑
        if (S < 0.f)
        {
            S = 0.f;
            T = FMath::Clamp(F / E, 0.f, 1.f);
        }
        else if (S > 1.f)
        {
            S = 1.f;
            T = FMath::Clamp((B + F) / E, 0.f, 1.f);
        }
        
        // T 클램핑 (S 재계산 필요)
        if (T < 0.f)
        {
            T = 0.f;
            S = FMath::Clamp(-C / A, 0.f, 1.f);
        }
        else if (T > 1.f)
        {
            T = 1.f;
            S = FMath::Clamp((B - C) / A, 0.f, 1.f);
        }
        
        OutTA = S;
        OutTB = T;
    }
    
    float SegmentSegmentDistanceSquared(
        const FVector& A1, const FVector& A2,
        const FVector& B1, const FVector& B2)
    {
        float TA, TB;
        ClosestPointsOnSegments(A1, A2, B1, B2, TA, TB);
        
        const FVector PointA = A1 + (A2 - A1) * TA;
        const FVector PointB = B1 + (B2 - B1) * TB;
        
        return DistanceSquared(PointA, PointB);
    }
    
    float SegmentSegmentDistanceSquaredWithPoints(
        const FVector& A1, const FVector& A2,
        const FVector& B1, const FVector& B2,
        FVector& OutPointA, FVector& OutPointB)
    {
        float TA, TB;
        ClosestPointsOnSegments(A1, A2, B1, B2, TA, TB);
        
        OutPointA = A1 + (A2 - A1) * TA;
        OutPointB = B1 + (B2 - B1) * TB;
        
        return DistanceSquared(OutPointA, OutPointB);
    }
    
    bool RayCylinderIntersection(
        const FVector& RayOrigin,
        const FVector& RayDir,
        const FVector& CylinderA,
        const FVector& CylinderB,
        float CylinderRadius,
        float& OutT,
        float& OutAxisT)
    {
        const FVector CylinderAxis = CylinderB - CylinderA;
        const float CylinderLenSq = LengthSquared(CylinderAxis);
        
        // 실린더가 점으로 퇴화
        if (CylinderLenSq < KINDA_SMALL_NUMBER)
        {
            // 구로 처리
            OutT = RaySphereIntersection(RayOrigin, RayDir, CylinderA, CylinderRadius);
            OutAxisT = 0.f;
            return OutT >= 0.f;
        }
        
        const FVector CylinderDir = CylinderAxis * FMath::InvSqrt(CylinderLenSq);
        const float CylinderLen = FMath::Sqrt(CylinderLenSq);
        
        // 레이를 실린더 로컬 좌표로 변환
        const FVector Delta = RayOrigin - CylinderA;
        
        // 레이 방향에서 실린더 축 방향 성분 제거
        const float RayDotAxis = FVector::DotProduct(RayDir, CylinderDir);
        const FVector RayPerp = RayDir - CylinderDir * RayDotAxis;
        
        // 델타에서 실린더 축 방향 성분 제거
        const float DeltaDotAxis = FVector::DotProduct(Delta, CylinderDir);
        const FVector DeltaPerp = Delta - CylinderDir * DeltaDotAxis;
        
        // 2D 원-레이 교차 테스트 (축에 수직인 평면에서)
        const float A = LengthSquared(RayPerp);
        
        // 레이가 축과 평행
        if (A < KINDA_SMALL_NUMBER)
        {
            // 원 안에 있는지 확인
            if (LengthSquared(DeltaPerp) <= CylinderRadius * CylinderRadius)
            {
                // 축 방향으로 교차점 계산
                if (FMath::Abs(RayDotAxis) > KINDA_SMALL_NUMBER)
                {
                    const float T1 = -DeltaDotAxis / RayDotAxis;
                    const float T2 = (CylinderLen - DeltaDotAxis) / RayDotAxis;
                    OutT = FMath::Min(T1, T2);
                    if (OutT < 0.f) OutT = FMath::Max(T1, T2);
                    OutAxisT = (DeltaDotAxis + RayDotAxis * OutT) / CylinderLen;
                    return OutT >= 0.f;
                }
            }
            return false;
        }
        
        const float B = FVector::DotProduct(RayPerp, DeltaPerp);
        const float C = LengthSquared(DeltaPerp) - CylinderRadius * CylinderRadius;
        const float Discriminant = B * B - A * C;
        
        if (Discriminant < 0.f)
        {
            return false;
        }
        
        const float SqrtD = FMath::Sqrt(Discriminant);
        float T = (-B - SqrtD) / A;
        
        if (T < 0.f)
        {
            T = (-B + SqrtD) / A;
        }
        
        if (T < 0.f)
        {
            return false;
        }
        
        // 축 상의 위치 계산
        OutAxisT = (DeltaDotAxis + RayDotAxis * T) / CylinderLen;
        OutT = T;
        
        return true;
    }
}
