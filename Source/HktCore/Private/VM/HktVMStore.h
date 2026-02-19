// Copyright Hkt Studios, Inc. All Rights Reserved.
// [Flat SOA Refactor] - FHktVMStore: open-addressing hash LocalCache

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"

/**
 * FHktVMStore - VM의 로컬 데이터 뷰 (Internal)
 *
 * [변경사항]
 * - LocalCache: TArray linear search → 고정 크기 open-addressing hash
 *   VM 수명 내 항목 수 < 64이므로 64슬롯이면 충분 (load factor < 100%)
 * - WorldState 읽기: Flat SOA의 GetProperty() 활용
 * - 트랜잭션 격리 + read-after-write 일관성 유지 (역할 변경 없음)
 */
struct FHktVMStore
{
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;

    // ========================================================================
    // 공개 인터페이스 (기존과 동일)
    // ========================================================================

    int32 Read(uint16 PropertyId) const;
    int32 ReadEntity(FHktEntityId Entity, uint16 PropertyId) const;
    void Write(uint16 PropertyId, int32 Value);
    void WriteEntity(FHktEntityId Entity, uint16 PropertyId, int32 Value);

    // --- PendingWrites (ApplyStoreSystem이 소비) ---
    struct FPendingWrite
    {
        uint16 PropertyId;
        FHktEntityId Entity;
        int32 Value;
    };
    TArray<FPendingWrite> PendingWrites;  // Reserve(MaxPendingWritesPerVM)

    void ClearPendingWrites();
    void Reset();

    /** WorldState 직접 참조 (Flat SOA 읽기용) */
    const FHktWorldState* WorldState = nullptr;

    // ========================================================================
    // [최적화] Open-Addressing Hash LocalCache
    // ========================================================================
    //
    // 기존: TArray<FLocalCacheEntry> + linear search O(N)
    // 변경: 고정 64슬롯 open-addressing hash O(1) 평균
    //
    // Key = (EntityId << 16) | PropertyId
    // 충돌 해결: linear probing
    // 크기: 64 * 16바이트 = 1KB (스택 친화적)
    //

    static constexpr int32 CacheCapacity = 64;
    static constexpr int32 CacheMask = CacheCapacity - 1;
    static constexpr uint64 EmptyKey = ~0ULL;

    struct FCacheSlot
    {
        uint64 Key = EmptyKey;
        int32 Value = 0;
    };

    FCacheSlot LocalCache[CacheCapacity];
    int32 CacheCount = 0;  // 삽입된 항목 수 (오버플로우 방지용)

private:
    static FORCEINLINE uint64 MakeCacheKey(FHktEntityId Entity, uint16 PropertyId)
    {
        return (static_cast<uint64>(static_cast<uint32>(Entity)) << 16) | PropertyId;
    }

    /** hash → 시작 인덱스 (fibonacci hashing) */
    static FORCEINLINE int32 HashSlot(uint64 Key)
    {
        // Knuth multiplicative hash
        return static_cast<int32>((Key * 11400714819323198485ULL) >> 58) & CacheMask;
    }

    /** 캐시에서 키 검색. 찾으면 Value 반환, 없으면 INT32_MIN 반환 */
    FORCEINLINE int32 CacheLookup(uint64 Key) const
    {
        int32 Slot = HashSlot(Key);
        for (int32 i = 0; i < CacheCapacity; ++i)
        {
            const int32 Idx = (Slot + i) & CacheMask;
            if (LocalCache[Idx].Key == Key)
                return LocalCache[Idx].Value;
            if (LocalCache[Idx].Key == EmptyKey)
                return INT32_MIN; // not found
        }
        return INT32_MIN;
    }

    /** 캐시에 삽입 또는 갱신 */
    FORCEINLINE void CacheInsert(uint64 Key, int32 Value)
    {
        int32 Slot = HashSlot(Key);
        for (int32 i = 0; i < CacheCapacity; ++i)
        {
            const int32 Idx = (Slot + i) & CacheMask;
            if (LocalCache[Idx].Key == Key)
            {
                // 기존 키 갱신
                LocalCache[Idx].Value = Value;
                return;
            }
            if (LocalCache[Idx].Key == EmptyKey)
            {
                // 빈 슬롯에 삽입
                if (CacheCount < CacheCapacity - 1) // 마지막 1슬롯은 비워둠 (탐색 종료 보장)
                {
                    LocalCache[Idx].Key = Key;
                    LocalCache[Idx].Value = Value;
                    ++CacheCount;
                }
                return;
            }
        }
        // 풀 — 무시 (CacheCount 제한으로 도달하지 않음)
    }
};
