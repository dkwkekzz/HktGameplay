// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * HktDeterministicMath - 결정론적 수학 유틸리티
 *
 * 시뮬레이션 전반에서 사용되는 순수 수학 함수.
 * HktCore의 결정론 보장을 위해 플랫폼 독립적으로 구현.
 */
namespace HktMath
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

        if (ABLenSq < KINDA_SMALL_NUMBER)
        {
            return SegA;
        }

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
    // 레이 교차 연산
    // ========================================================================

    /**
     * 레이-구 교차 테스트
     * @return 최초 교차점까지의 거리 (음수면 교차 없음)
     */
    FORCEINLINE float RaySphereIntersection(
        const FVector& RayOrigin,
        const FVector& RayDir,
        const FVector& SphereCenter,
        float SphereRadius)
    {
        const FVector OC = RayOrigin - SphereCenter;

        const float B = FVector::DotProduct(OC, RayDir);
        const float C = LengthSquared(OC) - SphereRadius * SphereRadius;
        const float Discriminant = B * B - C;

        if (Discriminant < 0.f)
        {
            return -1.f;
        }

        const float SqrtD = FMath::Sqrt(Discriminant);

        float T = -B - SqrtD;
        if (T < 0.f)
        {
            T = -B + SqrtD;
        }

        return T >= 0.f ? T : -1.f;
    }

    // ========================================================================
    // 선분-선분 연산 (구현은 cpp에서)
    // ========================================================================

    /**
     * 두 선분의 최근접점 쌍
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

    /**
     * 레이-무한 실린더 교차 테스트 (캡슐 측면용)
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
