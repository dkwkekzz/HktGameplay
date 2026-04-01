// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Data/HktVoxelRenderCache.h"
#include "HktVoxelCoreLog.h"

void FHktVoxelRenderCache::ApplyVoxelDelta(const FIntVector& ChunkCoord, uint16 LocalIndex, FHktVoxel NewValue)
{
	FScopeLock Lock(&ChunkLock);

	TUniquePtr<FHktVoxelChunk>* Found = Chunks.Find(ChunkCoord);
	if (!Found)
	{
		UE_LOG(LogHktVoxelCore, Warning, TEXT("ApplyVoxelDelta: Chunk (%d,%d,%d) not loaded"),
			ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z);
		return;
	}

	FHktVoxelChunk* Chunk = Found->Get();
	FIntVector Local = FHktVoxelChunk::IndexToLocal(LocalIndex);
	Chunk->At(Local.X, Local.Y, Local.Z) = NewValue;
	Chunk->bMeshDirty = true;
	Chunk->bMeshReady = false;
}

void FHktVoxelRenderCache::LoadChunk(const FIntVector& ChunkCoord, const FHktVoxel* VoxelData, int32 VoxelCount)
{
	FScopeLock Lock(&ChunkLock);

	TUniquePtr<FHktVoxelChunk> NewChunk = MakeUnique<FHktVoxelChunk>();
	NewChunk->ChunkCoord = ChunkCoord;
	NewChunk->bMeshDirty = true;
	NewChunk->bMeshReady = false;

	const int32 MaxVoxels = FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE * FHktVoxelChunk::SIZE;
	const int32 CopyCount = FMath::Min(VoxelCount, MaxVoxels);

	if (VoxelData && CopyCount > 0)
	{
		FMemory::Memcpy(NewChunk->Data, VoxelData, CopyCount * sizeof(FHktVoxel));
	}

	Chunks.Add(ChunkCoord, MoveTemp(NewChunk));

	UE_LOG(LogHktVoxelCore, Verbose, TEXT("LoadChunk: (%d,%d,%d) loaded with %d voxels"),
		ChunkCoord.X, ChunkCoord.Y, ChunkCoord.Z, CopyCount);
}

void FHktVoxelRenderCache::UnloadChunk(const FIntVector& ChunkCoord)
{
	FScopeLock Lock(&ChunkLock);
	Chunks.Remove(ChunkCoord);
}

void FHktVoxelRenderCache::GetDirtyChunks(TArray<FIntVector>& OutDirtyChunks) const
{
	FScopeLock Lock(&ChunkLock);
	OutDirtyChunks.Reset();

	for (const auto& Pair : Chunks)
	{
		if (Pair.Value->bMeshDirty)
		{
			OutDirtyChunks.Add(Pair.Key);
		}
	}
}

FHktVoxelChunk* FHktVoxelRenderCache::GetChunk(const FIntVector& ChunkCoord)
{
	FScopeLock Lock(&ChunkLock);
	TUniquePtr<FHktVoxelChunk>* Found = Chunks.Find(ChunkCoord);
	return Found ? Found->Get() : nullptr;
}

const FHktVoxelChunk* FHktVoxelRenderCache::GetChunk(const FIntVector& ChunkCoord) const
{
	FScopeLock Lock(&ChunkLock);
	const TUniquePtr<FHktVoxelChunk>* Found = Chunks.Find(ChunkCoord);
	return Found ? Found->Get() : nullptr;
}

int32 FHktVoxelRenderCache::GetChunkCount() const
{
	FScopeLock Lock(&ChunkLock);
	return Chunks.Num();
}

void FHktVoxelRenderCache::Clear()
{
	FScopeLock Lock(&ChunkLock);
	Chunks.Empty();
}
