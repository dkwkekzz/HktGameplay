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

FHktVoxelMeshScheduler::~FHktVoxelMeshScheduler()
{
	Flush();
}

void FHktVoxelMeshScheduler::Flush()
{
	for (UE::Tasks::FTask& Task : PendingTasks)
	{
		Task.Wait();
	}
	PendingTasks.Empty();
}

void FHktVoxelMeshScheduler::Tick(const FVector& CameraPos)
{
	if (!RenderCache)
	{
		return;
	}

	// 완료된 태스크 제거
	PendingTasks.RemoveAllSwap([](const UE::Tasks::FTask& Task) { return Task.IsCompleted(); });

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

		// dirty 해제 + 세대 캡처하여 메싱 중 새 delta가 들어오면 결과를 버릴 수 있도록
		Chunk->bMeshDirty.store(false, std::memory_order_relaxed);
		const uint32 Gen = Chunk->MeshGeneration.load(std::memory_order_acquire);

		PendingTasks.Add(UE::Tasks::Launch(
			TEXT("HktVoxelMeshing"),
			[Chunk, Gen]()
			{
				FHktVoxelMesher::MeshChunk(*Chunk);
				// 세대가 변경되지 않았을 때만 결과를 유효로 마킹
				if (Chunk->MeshGeneration.load(std::memory_order_acquire) == Gen)
				{
					Chunk->bMeshReady.store(true, std::memory_order_release);
				}
			},
			UE::Tasks::ETaskPriority::BackgroundNormal
		));
	}
}

FVector FHktVoxelMeshScheduler::ChunkToWorld(const FIntVector& ChunkCoord)
{
	// 청크 중심 위치 = 청크좌표 * 청크크기 * 복셀크기 + 오프셋
	// 복셀 크기 15 유닛 = 15cm (UE5 기본 단위 = cm)
	static constexpr float VoxelSize = FHktVoxelChunk::VOXEL_SIZE;
	static constexpr float ChunkWorldSize = FHktVoxelChunk::SIZE * VoxelSize;
	static constexpr float HalfChunk = ChunkWorldSize * 0.5f;

	return FVector(
		ChunkCoord.X * ChunkWorldSize + HalfChunk,
		ChunkCoord.Y * ChunkWorldSize + HalfChunk,
		ChunkCoord.Z * ChunkWorldSize + HalfChunk
	);
}
