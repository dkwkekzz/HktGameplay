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

		// Tile Texture (Phase 1)
		TileEnabledParam.Bind(ParameterMap, TEXT("HktTileEnabled"));
		TileArrayParam.Bind(ParameterMap, TEXT("HktTileArray"));
		TileArraySamplerParam.Bind(ParameterMap, TEXT("HktTileSampler"));
		TileIndexLUTParam.Bind(ParameterMap, TEXT("HktTileIndexLUT"));
		TileIndexLUTSamplerParam.Bind(ParameterMap, TEXT("HktTileIndexLUTSampler"));

		// Material LUT (Phase 2)
		MaterialLUTEnabledParam.Bind(ParameterMap, TEXT("HktMaterialLUTEnabled"));
		MaterialLUTParam.Bind(ParameterMap, TEXT("HktMaterialLUT"));
		MaterialLUTSamplerParam.Bind(ParameterMap, TEXT("HktMaterialLUTSampler"));
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

		// Tile Texture (Phase 1) — HktTileEnabled으로 셰이더 분기 제어
		const bool bTileEnabled = (VoxelVF->TileArrayRHI != nullptr && VoxelVF->TileIndexLUTRHI != nullptr);
		if (TileEnabledParam.IsBound())
		{
			ShaderBindings.Add(TileEnabledParam, bTileEnabled ? 1.0f : 0.0f);
		}
		if (TileArrayParam.IsBound() && VoxelVF->TileArrayRHI)
		{
			ShaderBindings.Add(TileArrayParam, VoxelVF->TileArrayRHI);
		}
		if (TileArraySamplerParam.IsBound() && VoxelVF->TileArraySamplerRHI)
		{
			ShaderBindings.Add(TileArraySamplerParam, VoxelVF->TileArraySamplerRHI);
		}
		if (TileIndexLUTParam.IsBound() && VoxelVF->TileIndexLUTRHI)
		{
			ShaderBindings.Add(TileIndexLUTParam, VoxelVF->TileIndexLUTRHI);
		}
		if (TileIndexLUTSamplerParam.IsBound() && VoxelVF->TileIndexLUTSamplerRHI)
		{
			ShaderBindings.Add(TileIndexLUTSamplerParam, VoxelVF->TileIndexLUTSamplerRHI);
		}

		// Material LUT (Phase 2) — nullptr이면 기존 하드코딩 PBR 폴백
		const bool bMaterialLUTEnabled = (VoxelVF->MaterialLUTRHI != nullptr);
		if (MaterialLUTEnabledParam.IsBound())
		{
			ShaderBindings.Add(MaterialLUTEnabledParam, bMaterialLUTEnabled ? 1.0f : 0.0f);
		}
		if (MaterialLUTParam.IsBound() && VoxelVF->MaterialLUTRHI)
		{
			ShaderBindings.Add(MaterialLUTParam, VoxelVF->MaterialLUTRHI);
		}
		if (MaterialLUTSamplerParam.IsBound() && VoxelVF->MaterialLUTSamplerRHI)
		{
			ShaderBindings.Add(MaterialLUTSamplerParam, VoxelVF->MaterialLUTSamplerRHI);
		}
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, PaletteTextureParam);
	LAYOUT_FIELD(FShaderResourceParameter, PaletteSamplerParam);

	// Tile Texture (Phase 1)
	LAYOUT_FIELD(FShaderParameter, TileEnabledParam);
	LAYOUT_FIELD(FShaderResourceParameter, TileArrayParam);
	LAYOUT_FIELD(FShaderResourceParameter, TileArraySamplerParam);
	LAYOUT_FIELD(FShaderResourceParameter, TileIndexLUTParam);
	LAYOUT_FIELD(FShaderResourceParameter, TileIndexLUTSamplerParam);

	// Material LUT (Phase 2)
	LAYOUT_FIELD(FShaderParameter, MaterialLUTEnabledParam);
	LAYOUT_FIELD(FShaderResourceParameter, MaterialLUTParam);
	LAYOUT_FIELD(FShaderResourceParameter, MaterialLUTSamplerParam);
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

void FHktVoxelVertexFactory::SetTileTextures(
	FRHITexture* InTileArray, FRHISamplerState* InTileSampler,
	FRHITexture* InTileIndexLUT, FRHISamplerState* InLUTSampler)
{
	TileArrayRHI = InTileArray;
	TileArraySamplerRHI = InTileSampler;
	TileIndexLUTRHI = InTileIndexLUT;
	TileIndexLUTSamplerRHI = InLUTSampler;
}

void FHktVoxelVertexFactory::SetMaterialLUT(FRHITexture* InLUT, FRHISamplerState* InSampler)
{
	MaterialLUTRHI = InLUT;
	MaterialLUTSamplerRHI = InSampler;
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
