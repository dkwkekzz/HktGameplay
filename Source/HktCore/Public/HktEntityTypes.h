// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Crc.h"

namespace Hkt
{
    // ==================================================================================
    // Common Types (기본 자료형)
    // ==================================================================================

    using FEntityID = uint32;
    constexpr FEntityID InvalidEntityID = 0;

    struct FVector3
    {
        float X, Y, Z;
        FVector3() : X(0), Y(0), Z(0) {}
        FVector3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
        bool operator==(const FVector3& Other) const { return X == Other.X && Y == Other.Y && Z == Other.Z; }
        FVector3 operator+(const FVector3& Other) const { return FVector3(X + Other.X, Y + Other.Y, Z + Other.Z); }
        FVector3 operator-(const FVector3& Other) const { return FVector3(X - Other.X, Y - Other.Y, Z - Other.Z); }
        FVector3 operator*(float Scalar) const { return FVector3(X * Scalar, Y * Scalar, Z * Scalar); }
    };

    struct FCellCoord
    {
        int32 X, Y;
        bool operator==(const FCellCoord& Other) const { return X == Other.X && Y == Other.Y; }
        friend uint32 GetTypeHash(const FCellCoord& C) { return HashCombine(::GetTypeHash(C.X), ::GetTypeHash(C.Y)); }
    };

    /** SOA 구조의 컴포넌트 데이터 */
    struct FComponentData
    {
        FVector3 Position;
        int32 Health;
    };
}
