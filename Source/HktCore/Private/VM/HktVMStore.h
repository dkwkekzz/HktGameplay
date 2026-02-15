// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"

/**
 * FHktVMStore - VM의 로컬 데이터 뷰 (Internal)
 *
 * 읽기: 로컬 캐시 → WorldState 순으로 조회
 * 쓰기: 로컬 캐시 + PendingWrites에 기록
 * VM 완료 시 PendingWrites가 WorldState에 일괄 적용
 */
struct FHktVMStore
{
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;

    int32 Read(uint16 PropertyId) const;
    int32 ReadEntity(FHktEntityId Entity, uint16 PropertyId) const;

    void Write(uint16 PropertyId, int32 Value);
    void WriteEntity(FHktEntityId Entity, uint16 PropertyId, int32 Value);

    // flat PendingWrites (PropertyId 포함)
    struct FPendingWrite
    {
        uint16 PropertyId;
        FHktEntityId Entity;
        int32 Value;
    };
    TArray<FPendingWrite> PendingWrites;  // Reserve(MaxPendingWritesPerVM)

    /** 로컬 캐시 (VM 내 읽기/쓰기 일관성) — flat linear search */
    struct FLocalCacheEntry
    {
        uint64 Key;
        int32 Value;
    };
    TArray<FLocalCacheEntry> LocalCache;  // Reserve(MaxLocalCachePerVM)

    void ClearPendingWrites();
    void Reset();

    /** WorldState 직접 참조 (SOA 읽기용) */
    const FHktWorldState* WorldState = nullptr;

private:
    static uint64 MakeCacheKey(FHktEntityId Entity, uint16 PropertyId)
    {
        return (static_cast<uint64>(static_cast<uint32>(Entity)) << 16) | PropertyId;
    }
};
