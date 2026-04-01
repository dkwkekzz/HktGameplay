// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Meshing/HktVoxelMeshScheduler.h"
#include "Meshing/HktVoxelMesher.h"
#include "Data/HktVoxelRenderCache.h"
#include "Data/HktVoxelTypes.h"
#include "HktVoxelCoreLog.h"
#include "Tasks/Task.h"

FHktVoxelMeshScheduler::FHktVoxelMeshScheduler(FHktVoxelRenderCache* InRenderCache)
	: RenderCache(InRenderCache)
{
}

void FHktVoxelMeshScheduler::Tick(const FVector& CameraPos)
{
	if (!RenderCache)
	{
		return;
	}

	TArray<FIntVector> DirtyChunks;
	RenderCache->GetDirtyChunks(DirtyChunks);

	if (DirtyChunks.Num() == 0)
	{
		return;
	}

	// 카메라 거리 기준 우선순위 정렬 (가까운 청크 먼저)
	DirtyChunks.Sort([&](const FIntVector& A, const FIntVector& B)
	{
		return FVector::DistSquared(ChunkToWorld(A), CameraPos)
			 < FVector::DistSquared(ChunkToWorld(B), CameraPos);
	});

	const int32 Count = FMath::Min(DirtyChunks.Num(), MaxMeshPerFrame);
	for (int32 i = 0; i < Count; i++)
	{
		FHktVoxelChunk* Chunk = RenderCache->GetChunk(DirtyChunks[i]);
		if (!Chunk || !Chunk->bMeshDirty)
		{
			continue;
		}

		// dirty 플래그를 먼저 해제하여 중복 스케줄링 방지
		Chunk->bMeshDirty = false;

		UE::Tasks::Launch(
			TEXT("HktVoxelMeshing"),
			[Chunk]() { FHktVoxelMesher::MeshChunk(*Chunk); },
			UE::Tasks::ETaskPriority::BackgroundNormal
		);
	}
}

FVector FHktVoxelMeshScheduler::ChunkToWorld(const FIntVector& ChunkCoord)
{
	// 청크 중심 위치 = 청크좌표 * 청크크기 * 복셀크기 + 오프셋
	// 복셀 크기 100 유닛 (UE5 기본 단위 = cm)
	static constexpr float VoxelSize = 100.0f;
	static constexpr float ChunkWorldSize = FHktVoxelChunk::SIZE * VoxelSize;
	static constexpr float HalfChunk = ChunkWorldSize * 0.5f;

	return FVector(
		ChunkCoord.X * ChunkWorldSize + HalfChunk,
		ChunkCoord.Y * ChunkWorldSize + HalfChunk,
		ChunkCoord.Z * ChunkWorldSize + HalfChunk
	);
}
