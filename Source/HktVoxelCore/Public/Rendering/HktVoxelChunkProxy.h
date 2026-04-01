// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "Meshing/HktVoxelVertex.h"

class UHktVoxelChunkComponent;
class FHktVoxelVertexFactory;

/**
 * FHktVoxelChunkProxy — 복셀 청크 SceneProxy
 *
 * 하나의 청크(32x32x32)를 GPU 버퍼로 관리하고 렌더링한다.
 * 메싱 완료 시 Game Thread에서 ENQUEUE_RENDER_COMMAND로
 * UpdateMeshData_RenderThread를 호출하여 버퍼를 갱신한다.
 *
 * Phase 3 (프로덕션) 에서 사용. Phase 1은 PMC 기반.
 */
class HKTVOXELCORE_API FHktVoxelChunkProxy : public FPrimitiveSceneProxy
{
public:
	explicit FHktVoxelChunkProxy(const UHktVoxelChunkComponent* InComponent);
	virtual ~FHktVoxelChunkProxy();

	// FPrimitiveSceneProxy
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	SIZE_T GetAllocatedSize() const { return 0; }

	/** Render Thread에서 호출 — 메싱 완료 데이터로 GPU 버퍼 갱신 */
	void UpdateMeshData_RenderThread(
		const TArray<FHktVoxelVertex>& Vertices,
		const TArray<uint32>& Indices);

private:
	FBufferRHIRef VertexBuffer;
	FBufferRHIRef IndexBuffer;
	FHktVoxelVertexFactory* VertexFactory = nullptr;
	UMaterialInterface* VoxelMaterial = nullptr;
	int32 NumIndices = 0;
	int32 NumVertices = 0;
};
