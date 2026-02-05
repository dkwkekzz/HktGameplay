// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktTypes.generated.h"

// ============================================================================
// [Part 1] Entity Identifier
// ============================================================================

/** 엔티티 식별자 (WorldState 내 엔티티 인덱스) */
USTRUCT(BlueprintType)
struct FHktEntityId
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entity")
    int32 RawValue;

    constexpr FHktEntityId() : RawValue(0) {}
    constexpr FHktEntityId(int32 InValue) : RawValue(InValue) {}

    bool IsValid() const { return RawValue >= 0; }
    int32 GetValue() const { return RawValue; }

    FORCEINLINE bool operator==(const FHktEntityId& Other) const { return RawValue == Other.RawValue; }
    FORCEINLINE bool operator!=(const FHktEntityId& Other) const { return RawValue != Other.RawValue; }
    FORCEINLINE bool operator==(int32 InRawValue) const { return RawValue == InRawValue; }
    FORCEINLINE bool operator!=(int32 InRawValue) const { return RawValue != InRawValue; }

    operator int32() const { return RawValue; }

    friend uint32 GetTypeHash(const FHktEntityId& EntityId)
    {
        return GetTypeHash(EntityId.RawValue);
    }

    friend FArchive& operator<<(FArchive& Ar, FHktEntityId& Value)
    {
        return Ar << Value.RawValue;
    }
};

constexpr FHktEntityId InvalidEntityId = FHktEntityId(INDEX_NONE);

/** 유효하지 않은 셀 (엔티티가 아직 위치가 없을 때) */
const FIntPoint InvalidCell = FIntPoint(INT_MAX, INT_MAX);

// ============================================================================
// [Part 2] VM Handle & Register Types
// ============================================================================

/** VM 핸들 (RuntimePool 내 슬롯 인덱스 + Generation) */
struct FHktVMHandle
{
    uint32 Index : 24;
    uint32 Generation : 8;

    static constexpr FHktVMHandle Invalid() { return {0xFFFFFF, 0}; }
    bool IsValid() const { return Index != 0xFFFFFF; }

    bool operator==(const FHktVMHandle& Other) const
    {
        return Index == Other.Index && Generation == Other.Generation;
    }
};

/** 레지스터 인덱스 */
using RegisterIndex = uint8;
constexpr int32 MaxRegisters = 16;

/** 레거시 별칭 (HktCore 내부 호환용) */
using EntityId = FHktEntityId;

/**
 * Reg - 특수 레지스터 별칭
 *
 * R0~R9는 범용 레지스터
 * R10~R15는 특수 목적 레지스터
 */
namespace Reg
{
    // 범용 레지스터
    constexpr RegisterIndex R0 = 0;
    constexpr RegisterIndex R1 = 1;
    constexpr RegisterIndex R2 = 2;
    constexpr RegisterIndex R3 = 3;
    constexpr RegisterIndex R4 = 4;
    constexpr RegisterIndex R5 = 5;
    constexpr RegisterIndex R6 = 6;
    constexpr RegisterIndex R7 = 7;
    constexpr RegisterIndex R8 = 8;
    constexpr RegisterIndex R9 = 9;
    constexpr RegisterIndex Temp = 9;       // 임시 레지스터 (R9 별칭)

    // 특수 목적 레지스터
    constexpr RegisterIndex Self = 10;      // 현재 엔티티 (IntentEvent.SourceEntity)
    constexpr RegisterIndex Target = 11;    // 타겟 엔티티 (IntentEvent.TargetEntity)
    constexpr RegisterIndex Spawned = 12;   // 최근 생성된 엔티티
    constexpr RegisterIndex Hit = 13;       // 충돌 대상 엔티티
    constexpr RegisterIndex Iter = 14;      // ForEach 순회용 (NextFound 결과)
    constexpr RegisterIndex Flag = 15;      // 범용 플래그/상태
    constexpr RegisterIndex Count = 15;     // 카운트 (Flag와 동일 슬롯)
}

// ============================================================================
// [Part 3] Core Constants
// ============================================================================

namespace HktCore
{
    constexpr int32 MaxEntities = 1024;
    constexpr int32 MaxProperties = 128;
}

// ============================================================================
// [Part 4] Entity Type Constants
// ============================================================================

namespace HktEntityType
{
    constexpr int32 None = 0;
    constexpr int32 Unit = 1;
    constexpr int32 Projectile = 2;
    constexpr int32 Equipment = 3;
    constexpr int32 Building = 4;
}
