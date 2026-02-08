#pragma once

#include "CoreMinimal.h"
#include "HktCoreTypes.h"

// Forward declaration
class IHktStashInterface;

/**
 * FHktVMStore - VM의 로컬 데이터 뷰 (Internal)
 * 
 * 읽기: 로컬 캐시 → Stash 순으로 조회
 * 쓰기: 로컬 캐시 + PendingWrites에 기록
 * VM 완료 시 PendingWrites가 Stash에 일괄 적용
 */
struct FHktVMStore
{
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;
    
    int32 Read(uint16 PropertyId) const;
    int32 ReadEntity(FHktEntityId Entity, uint16 PropertyId) const;
    
    void Write(uint16 PropertyId, int32 Value);
    void WriteEntity(FHktEntityId Entity, uint16 PropertyId, int32 Value);
    
    struct FPendingWrite
    {
        FHktEntityId Entity;
        uint16 PropertyId;
        int32 Value;
    };
    TArray<FPendingWrite> PendingWrites;
    
    /** 로컬 캐시 (VM 내 읽기/쓰기 일관성) */
    TMap<uint64, int32> LocalCache;
    
    void ClearPendingWrites();
    void Reset();
    
    IHktStashInterface* Stash = nullptr;

private:
    static uint64 MakeCacheKey(FHktEntityId Entity, uint16 PropertyId)
    {
        return (static_cast<uint64>(Entity.RawValue) << 16) | PropertyId;
    }
};
