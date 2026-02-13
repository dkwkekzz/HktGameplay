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

    // SOA-batched writes (PropertyId별 묶음)
    struct FPendingWrite
    {
        FHktEntityId Entity;
        int32 Value;
    };
    TMap<uint16, TArray<FPendingWrite>> PendingWritesByProperty;

    /** 로컬 캐시 (VM 내 읽기/쓰기 일관성) */
    TMap<uint64, int32> LocalCache;

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
