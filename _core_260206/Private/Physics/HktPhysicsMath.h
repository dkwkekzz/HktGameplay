// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace HktPhysics
{
    // ========================================================================
    // 기본 벡터 연산 (인라인)
    // ========================================================================
    
    /** 거리 제곱 (sqrt 회피) */
    FORCEINLINE float DistanceSquared(const FVector& A, const FVector& B)
    {
        const FVector D = A - B;
        return D.X * D.X + D.Y * D.Y + D.Z * D.Z;
    }
    
    /** 거리 */
    FORCEINLINE float Distance(const FVector& A, const FVector& B)
    {
        return FMath::Sqrt(DistanceSquared(A, B));
    }
    
    /** 벡터 길이 제곱 */
    FORCEINLINE float LengthSquared(const FVector& V)
    {
        return V.X * V.X + V.Y * V.Y + V.Z * V.Z;
    }
    
    /** 안전한 정규화 (길이 0이면 ZeroVector 반환) */
    FORCEINLINE FVector SafeNormalize(const FVector& V, float Tolerance = KINDA_SMALL_NUMBER)
    {
        const float SizeSq = LengthSquared(V);
        if (SizeSq > Tolerance * Tolerance)
        {
            const float Scale = FMath::InvSqrt(SizeSq);
            return FVector(V.X * Scale, V.Y * Scale, V.Z * Scale);
        }
        return FVector::ZeroVector;
    }
    
    // ========================================================================
    // 점-선분 연산
    // ========================================================================
    
    /**
     * 점과 선분의 최근접점
     * @param Point - 점
     * @param SegA - 선분 시작점
     * @param SegB - 선분 끝점
     * @return 선분 위의 최근접점
     */
    FORCEINLINE FVector ClosestPointOnSegment(
        const FVector& Point, 
        const FVector& SegA, 
        const FVector& SegB)
    {
        const FVector AB = SegB - SegA;
        const float ABLenSq = LengthSquared(AB);
        
        // 퇴화 케이스: 선분이 점
        if (ABLenSq < KINDA_SMALL_NUMBER)
        {
            return SegA;
        }
        
        // 파라미터 T 계산 (0~1 범위로 클램프)
        float T = FVector::DotProduct(Point - SegA, AB) / ABLenSq;
        T = FMath::Clamp(T, 0.f, 1.f);
        
        return SegA + AB * T;
    }
    
    /**
     * 점과 선분의 최근접점 + 파라미터 T 반환
     */
    FORCEINLINE FVector ClosestPointOnSegmentWithT(
        const FVector& Point,
        const FVector& SegA,
        const FVector& SegB,
        float& OutT)
    {
        const FVector AB = SegB - SegA;
        const float ABLenSq = LengthSquared(AB);
        
        if (ABLenSq < KINDA_SMALL_NUMBER)
        {
            OutT = 0.f;
            return SegA;
        }
        
        OutT = FMath::Clamp(FVector::DotProduct(Point - SegA, AB) / ABLenSq, 0.f, 1.f);
        return SegA + AB * OutT;
    }
    
    // ========================================================================
    // 선분-선분 연산
    // ========================================================================
    
    /**
     * 두 선분의 최근접점 쌍
     * @param A1, A2 - 선분 A의 양 끝점
     * @param B1, B2 - 선분 B의 양 끝점
     * @param OutTA - 선분 A 상의 파라미터 (0~1)
     * @param OutTB - 선분 B 상의 파라미터 (0~1)
     */
    void ClosestPointsOnSegments(
        const FVector& A1, const FVector& A2,
        const FVector& B1, const FVector& B2,
        float& OutTA, float& OutTB);
    
    /**
     * 두 선분의 최근접 거리 제곱
     */
    float SegmentSegmentDistanceSquared(
        const FVector& A1, const FVector& A2,
        const FVector& B1, const FVector& B2);
    
    /**
     * 두 선분의 최근접 거리 제곱 + 최근접점 반환
     */
    float SegmentSegmentDistanceSquaredWithPoints(
        const FVector& A1, const FVector& A2,
        const FVector& B1, const FVector& B2,
        FVector& OutPointA, FVector& OutPointB);
    
    // ========================================================================
    // 레이 교차 연산
    // ========================================================================
    
    /**
     * 레이-구 교차 테스트
     * @param RayOrigin - 레이 시작점
     * @param RayDir - 레이 방향 (정규화됨)
     * @param SphereCenter - 구 중심
     * @param SphereRadius - 구 반경
     * @return 최초 교차점까지의 거리 (음수면 교차 없음)
     */
    FORCEINLINE float RaySphereIntersection(
        const FVector& RayOrigin,
        const FVector& RayDir,
        const FVector& SphereCenter,
        float SphereRadius)
    {
        // 레이 원점에서 구 중심까지의 벡터
        const FVector OC = RayOrigin - SphereCenter;
        
        // 이차방정식 계수: t^2 + 2bt + c = 0
        const float B = FVector::DotProduct(OC, RayDir);
        const float C = LengthSquared(OC) - SphereRadius * SphereRadius;
        const float Discriminant = B * B - C;
        
        // 판별식이 음수면 교차 없음
        if (Discriminant < 0.f)
        {
            return -1.f;
        }
        
        const float SqrtD = FMath::Sqrt(Discriminant);
        
        // 작은 근 먼저 (가까운 교차점)
        float T = -B - SqrtD;
        
        // T가 음수면 뒤쪽 교차점 확인
        if (T < 0.f)
        {
            T = -B + SqrtD;
        }
        
        return T >= 0.f ? T : -1.f;
    }
    
    /**
     * 레이-무한 실린더 교차 테스트 (캡슐 측면용)
     * @param RayOrigin - 레이 시작점
     * @param RayDir - 레이 방향 (정규화됨)
     * @param CylinderA - 실린더 축 시작점
     * @param CylinderB - 실린더 축 끝점
     * @param CylinderRadius - 실린더 반경
     * @param OutT - 교차 거리
     * @param OutAxisT - 실린더 축 상의 파라미터 (0~1 범위 외일 수 있음)
     * @return 교차 여부
     */
    bool RayCylinderIntersection(
        const FVector& RayOrigin,
        const FVector& RayDir,
        const FVector& CylinderA,
        const FVector& CylinderB,
        float CylinderRadius,
        float& OutT,
        float& OutAxisT);
}
