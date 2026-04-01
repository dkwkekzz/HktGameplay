// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Rendering/HktVoxelChunkProxy.h"
#include "Rendering/HktVoxelVertexFactory.h"
#include "Rendering/HktVoxelChunkComponent.h"
#include "HktVoxelCoreLog.h"
#include "Materials/Material.h"
#include "SceneManagement.h"
#include "RHIStaticStates.h"
#include "TextureResource.h"

FHktVoxelChunkProxy::FHktVoxelChunkProxy(const UHktVoxelChunkComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	// 머티리얼은 Component에서 0번 슬롯을 가져옴
	VoxelMaterial = InComponent->GetMaterial(0);
	if (!VoxelMaterial)
	{
		VoxelMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
}

FHktVoxelChunkProxy::~FHktVoxelChunkProxy()
{
	if (VertexFactory)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
		VertexFactory = nullptr;
	}
}

void FHktVoxelChunkProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	if (NumIndices == 0 || !VertexBufferWrapper.VertexBufferRHI.IsValid() || !IndexBufferWrapper.IndexBufferRHI.IsValid())
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (!(VisibilityMap & (1 << ViewIndex)))
		{
			continue;
		}

		FMeshBatch& Mesh = Collector.AllocateMesh();
		Mesh.VertexFactory = VertexFactory;
		Mesh.Type = PT_TriangleList;
		Mesh.bWireframe = false;
		Mesh.bUseWireframeSelectionColoring = false;
		Mesh.MaterialRenderProxy = VoxelMaterial->GetRenderProxy();
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.CastShadow = true;

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = &IndexBufferWrapper;
		BatchElement.NumPrimitives = NumIndices / 3;
		BatchElement.FirstIndex = 0;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = NumVertices > 0 ? NumVertices - 1 : 0;

		Collector.AddMesh(ViewIndex, Mesh);
	}
}

FPrimitiveViewRelevance FHktVoxelChunkProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	return Result;
}

void FHktVoxelChunkProxy::UpdateMeshData_RenderThread(
	const TArray<FHktVoxelVertex>& Vertices,
	const TArray<uint32>& Indices)
{
	check(IsInRenderingThread());

	if (Vertices.Num() == 0 || Indices.Num() == 0)
	{
		NumIndices = 0;
		VertexBufferWrapper.VertexBufferRHI = nullptr;
		IndexBufferWrapper.IndexBufferRHI = nullptr;
		return;
	}

	// Vertex Buffer 생성
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("HktVoxelChunkVB"));
		const uint32 Size = Vertices.Num() * sizeof(FHktVoxelVertex);
		VertexBufferWrapper.VertexBufferRHI = FRHICommandListImmediate::Get().CreateVertexBuffer(
			Size, BUF_Static, CreateInfo);

		void* Data = FRHICommandListImmediate::Get().LockBuffer(VertexBufferWrapper.VertexBufferRHI, 0, Size, RLM_WriteOnly);
		FMemory::Memcpy(Data, Vertices.GetData(), Size);
		FRHICommandListImmediate::Get().UnlockBuffer(VertexBufferWrapper.VertexBufferRHI);
	}

	// Index Buffer 생성
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("HktVoxelChunkIB"));
		const uint32 Size = Indices.Num() * sizeof(uint32);
		IndexBufferWrapper.IndexBufferRHI = FRHICommandListImmediate::Get().CreateIndexBuffer(
			sizeof(uint32), Size, BUF_Static, CreateInfo);

		void* Data = FRHICommandListImmediate::Get().LockBuffer(IndexBufferWrapper.IndexBufferRHI, 0, Size, RLM_WriteOnly);
		FMemory::Memcpy(Data, Indices.GetData(), Size);
		FRHICommandListImmediate::Get().UnlockBuffer(IndexBufferWrapper.IndexBufferRHI);
	}

	NumIndices = Indices.Num();
	NumVertices = Vertices.Num();

	// Vertex Factory 셋업
	if (!VertexFactory)
	{
		VertexFactory = new FHktVoxelVertexFactory(GetScene().GetFeatureLevel());
		VertexFactory->InitResource(FRHICommandListImmediate::Get());
	}

	// 팔레트 텍스처 — 커스텀 텍스처 미설정 시 GWhiteTexture 폴백 (흰색)
	if (!VertexFactory->PaletteTextureRHI)
	{
		VertexFactory->SetPaletteTexture(
			GWhiteTexture->TextureRHI,
			TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI());
	}

	FHktVoxelVertexFactory::FDataType VFData;
	VFData.PositionComponent = FVertexStreamComponent(
		&VertexBufferWrapper, 0, sizeof(FHktVoxelVertex), VET_UInt);
	VFData.MaterialComponent = FVertexStreamComponent(
		&VertexBufferWrapper, 4, sizeof(FHktVoxelVertex), VET_UInt);

	VertexFactory->SetData(VFData);
}
