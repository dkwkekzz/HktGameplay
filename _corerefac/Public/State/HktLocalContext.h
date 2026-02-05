// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/HktTypes.h"

// Forward declaration
class FHktWorldState;

/**
 * FHktLocalContext - 트랜잭션 스크래치패드 (Local Transactional Storage)
 *
 * VM 실행 중의 임시 작업 공간.
 * Write는 이곳에 기록되며, 실행이 성공적으로 끝나면
 * CommitChanges()를 통해 WorldState에 반영.
 *
 * 읽기: 로컬 캐시 → WorldState 순으로 조회
 * 쓰기: 로컬 캐시 + PendingWrites에 기록
 */
struct HKTCORE_API FHktLocalContext
{
    FHktEntityId SourceEntity = InvalidEntityId;
    FHktEntityId TargetEntity = InvalidEntityId;

    /** 현재 엔티티(SourceEntity) 기준 속성 읽기 */
    int32 Read(uint16 PropertyId) const;

    /** 특정 엔티티의 속성 읽기 */
    int32 ReadEntity(FHktEntityId Entity, uint16 PropertyId) const;

    /** 현재 엔티티(SourceEntity) 기준 속성 쓰기 */
    void Write(uint16 PropertyId, int32 Value);

    /** 특정 엔티티의 속성 쓰기 */
    void WriteEntity(FHktEntityId Entity, uint16 PropertyId, int32 Value);

    /** PendingWrites를 WorldState에 반영 */
    void CommitChanges();

    /** PendingWrites를 버리고 원래 상태로 복원 */
    void Rollback();

    /** 모든 상태 초기화 (재사용을 위해) */
    void Reset();

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

    FHktWorldState* WorldState = nullptr;

private:
    static uint64 MakeCacheKey(FHktEntityId Entity, uint16 PropertyId)
    {
        return (static_cast<uint64>(Entity.RawValue) << 16) | PropertyId;
    }
};
