// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Rendering/HktVoxelVertexFactory.h"
#include "MeshMaterialShader.h"
#include "ShaderParameterUtils.h"
#include "MeshDrawShaderBindings.h"

// ============================================================================
// 셰이더 파라미터 — HktPaletteTexture / HktPaletteSampler 바인딩
// ============================================================================

class FHktVoxelVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FHktVoxelVertexFactoryShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		PaletteTextureParam.Bind(ParameterMap, TEXT("HktPaletteTexture"));
		PaletteSamplerParam.Bind(ParameterMap, TEXT("HktPaletteSampler"));
		BoneMatricesParam.Bind(ParameterMap, TEXT("HktBoneMatrices"));
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const class FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const class FVertexFactory* VertexFactory,
		const struct FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		const FHktVoxelVertexFactory* VoxelVF = static_cast<const FHktVoxelVertexFactory*>(VertexFactory);

		if (PaletteTextureParam.IsBound() && VoxelVF->PaletteTextureRHI)
		{
			ShaderBindings.Add(PaletteTextureParam, VoxelVF->PaletteTextureRHI);
		}
		if (PaletteSamplerParam.IsBound() && VoxelVF->PaletteSamplerRHI)
		{
			ShaderBindings.Add(PaletteSamplerParam, VoxelVF->PaletteSamplerRHI);
		}
		if (BoneMatricesParam.IsBound() && VoxelVF->BoneTransformSRV)
		{
			ShaderBindings.Add(BoneMatricesParam, VoxelVF->BoneTransformSRV);
		}
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, PaletteTextureParam);
	LAYOUT_FIELD(FShaderResourceParameter, PaletteSamplerParam);
	LAYOUT_FIELD(FShaderResourceParameter, BoneMatricesParam);
};

IMPLEMENT_TYPE_LAYOUT(FHktVoxelVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHktVoxelVertexFactory, SF_Vertex, FHktVoxelVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FHktVoxelVertexFactory, SF_Pixel, FHktVoxelVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FHktVoxelVertexFactory, "/Plugin/HktVoxelCore/HktVoxelVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
);

FHktVoxelVertexFactory::FHktVoxelVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
	: FVertexFactory(InFeatureLevel)
{
}

void FHktVoxelVertexFactory::SetData(const FDataType& InData)
{
	Data = InData;
	UpdateRHI(FRHICommandListImmediate::Get());
}

void FHktVoxelVertexFactory::SetPaletteTexture(FRHITexture* InTexture, FRHISamplerState* InSampler)
{
	PaletteTextureRHI = InTexture;
	PaletteSamplerRHI = InSampler;
}

void FHktVoxelVertexFactory::SetBoneTransformSRV(FRHIShaderResourceView* InSRV)
{
	BoneTransformSRV = InSRV;
}

bool FHktVoxelVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	// Surface 도메인만 허용 (wireframe/debug 포함 — SpecialEngineMaterial 차단하면 와이어프레임 불가)
	return Parameters.MaterialParameters.MaterialDomain == MD_Surface;
}

void FHktVoxelVertexFactory::ModifyCompilationEnvironment(
	const FVertexFactoryShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("HKT_VOXEL_VERTEX_FACTORY"), TEXT("1"));
	OutEnvironment.SetDefine(TEXT("HKT_VOXEL_GPU_SKINNING"), TEXT("1"));
}

void FHktVoxelVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	FVertexDeclarationElementList Elements;

	// Stream 0: PackedPositionAndSize (uint32)
	if (Data.PositionComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
	}

	// Stream 1: PackedMaterialAndAO (uint32)
	if (Data.MaterialComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.MaterialComponent, 1));
	}

	InitDeclaration(Elements);
}

void FHktVoxelVertexFactory::ReleaseRHI()
{
	FVertexFactory::ReleaseRHI();
}
