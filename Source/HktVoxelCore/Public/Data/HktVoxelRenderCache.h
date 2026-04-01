// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/HktVoxelTypes.h"

/**
 * FHktVoxelRenderCache
 *
 * VM 복셀 상태의 UE5 측 읽기 전용 사본.
 * VM이 발행한 VoxelDelta를 적용하여 렌더링용 청크 데이터를 관리한다.
 *
 * 소유권 원칙:
 *   복셀 데이터의 유일한 소유자 = VM
 *   이 캐시는 "렌더링 전용 사본"으로 VM과 1~2틱 지연 허용
 *
 * 스레드 안전:
 *   ApplyVoxelDelta / LoadChunk / UnloadChunk = Game Thread에서 호출
 *   GetDirtyChunks / GetChunk = 메싱 스케줄러가 호출 (Game Thread)
 *   메싱 워커는 GetChunk()로 받은 포인터를 통해 청크 데이터를 읽고 메시 결과를 기록
 */
class HKTVOXELCORE_API FHktVoxelRenderCache
{
public:
	FHktVoxelRenderCache() = default;
	~FHktVoxelRenderCache() = default;

	// Non-copyable
	FHktVoxelRenderCache(const FHktVoxelRenderCache&) = delete;
	FHktVoxelRenderCache& operator=(const FHktVoxelRenderCache&) = delete;

	/** VM delta 적용 — 단일 복셀 변경 */
	void ApplyVoxelDelta(const FIntVector& ChunkCoord, uint16 LocalIndex, FHktVoxel NewValue);

	/** 초기 청크 로드 (VM에서 청크 전체 데이터 수신 시) */
	void LoadChunk(const FIntVector& ChunkCoord, const FHktVoxel* VoxelData, int32 VoxelCount);

	/** 청크 해제 */
	void UnloadChunk(const FIntVector& ChunkCoord);

	/** dirty 청크 목록 반환 (메싱 스케줄러용) */
	void GetDirtyChunks(TArray<FIntVector>& OutDirtyChunks) const;

	/** 메싱/렌더링용 청크 접근 */
	FHktVoxelChunk* GetChunk(const FIntVector& ChunkCoord);
	const FHktVoxelChunk* GetChunk(const FIntVector& ChunkCoord) const;

	/** 로드된 청크 수 */
	int32 GetChunkCount() const;

	/** 모든 청크 해제 */
	void Clear();

private:
	TMap<FIntVector, TUniquePtr<FHktVoxelChunk>> Chunks;
	mutable FCriticalSection ChunkLock;
};
