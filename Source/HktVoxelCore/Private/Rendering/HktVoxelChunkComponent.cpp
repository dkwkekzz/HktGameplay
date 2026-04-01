// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Rendering/HktVoxelChunkComponent.h"
#include "Rendering/HktVoxelChunkProxy.h"
#include "Data/HktVoxelRenderCache.h"
#include "Data/HktVoxelTypes.h"
#include "Meshing/HktVoxelVertex.h"
#include "HktVoxelCoreLog.h"

UHktVoxelChunkComponent::UHktVoxelChunkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CastShadow = true;
}

void UHktVoxelChunkComponent::Initialize(FHktVoxelRenderCache* Cache, const FIntVector& InChunkCoord)
{
	RenderCache = Cache;
	ChunkCoord = InChunkCoord;

	// 월드 위치를 청크 좌표에 맞게 설정
	static constexpr float VoxelSize = 100.0f;
	static constexpr float ChunkWorldSize = FHktVoxelChunk::SIZE * VoxelSize;

	SetWorldLocation(FVector(
		ChunkCoord.X * ChunkWorldSize,
		ChunkCoord.Y * ChunkWorldSize,
		ChunkCoord.Z * ChunkWorldSize));
}

void UHktVoxelChunkComponent::OnMeshReady()
{
	if (!RenderCache)
	{
		return;
	}

	const FHktVoxelChunk* Chunk = RenderCache->GetChunk(ChunkCoord);
	if (!Chunk || !Chunk->bMeshReady)
	{
		return;
	}

	// 메시 데이터 복사 (Game Thread → Render Thread 전달용)
	TArray<FHktVoxelVertex> VerticesCopy = Chunk->OpaqueVertices;
	TArray<uint32> IndicesCopy = Chunk->OpaqueIndices;

	FHktVoxelChunkProxy* Proxy = static_cast<FHktVoxelChunkProxy*>(SceneProxy);
	if (!Proxy)
	{
		MarkRenderStateDirty();
		return;
	}

	// GPU 버퍼 업로드는 반드시 Render Thread에서
	ENQUEUE_RENDER_COMMAND(HktVoxelUpdateMesh)(
		[Proxy, Verts = MoveTemp(VerticesCopy), Idxs = MoveTemp(IndicesCopy)](FRHICommandListImmediate& RHICmdList)
		{
			Proxy->UpdateMeshData_RenderThread(Verts, Idxs);
		}
	);
}

FPrimitiveSceneProxy* UHktVoxelChunkComponent::CreateSceneProxy()
{
	return new FHktVoxelChunkProxy(this);
}

FBoxSphereBounds UHktVoxelChunkComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	static constexpr float VoxelSize = 100.0f;
	static constexpr float ChunkWorldSize = FHktVoxelChunk::SIZE * VoxelSize;

	const FVector Extent(ChunkWorldSize * 0.5f);
	const FBox Box(-Extent, Extent);
	return FBoxSphereBounds(Box).TransformBy(LocalToWorld);
}
