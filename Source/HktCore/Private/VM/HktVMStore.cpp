// Copyright Hkt Studios, Inc. All Rights Reserved.
// [Flat SOA Refactor] - FHktVMStore 구현부

#include "HktVMStore.h"

// ============================================================================
// FHktVMStore
// ============================================================================

int32 FHktVMStore::Read(uint16 PropertyId) const
{
    return ReadEntity(SourceEntity, PropertyId);
}

int32 FHktVMStore::ReadEntity(FHktEntityId Entity, uint16 PropertyId) const
{
    // 1. LocalCache에서 O(1) 검색
    const uint64 Key = MakeCacheKey(Entity, PropertyId);
    const int32 Cached = CacheLookup(Key);
    if (Cached != INT32_MIN)
        return Cached;

    // 2. WorldState Flat SOA에서 읽기
    if (WorldState)
    {
        return WorldState->GetProperty(Entity, PropertyId);
    }
    return 0;
}

void FHktVMStore::Write(uint16 PropertyId, int32 Value)
{
    WriteEntity(SourceEntity, PropertyId, Value);
}

void FHktVMStore::WriteEntity(FHktEntityId Entity, uint16 PropertyId, int32 Value)
{
    const uint64 Key = MakeCacheKey(Entity, PropertyId);

    // LocalCache에 O(1) 삽입/갱신
    CacheInsert(Key, Value);

    // PendingWrites에 기록 (ApplyStoreSystem에서 일괄 커밋)
    PendingWrites.Add({ PropertyId, Entity, Value });
}

void FHktVMStore::ClearPendingWrites()
{
    PendingWrites.Reset();  // 용량 유지
}

void FHktVMStore::Reset()
{
    PendingWrites.Reset();  // 용량 유지

    // LocalCache 초기화 (전체 memset)
    for (int32 i = 0; i < CacheCapacity; ++i)
    {
        LocalCache[i].Key = EmptyKey;
        LocalCache[i].Value = 0;
    }
    CacheCount = 0;

    SourceEntity = InvalidEntityId;
    TargetEntity = InvalidEntityId;
}
