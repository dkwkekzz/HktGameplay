// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Rendering/HktVoxelVertexFactory.h"
#include "MeshMaterialShader.h"
#include "ShaderParameterUtils.h"

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

bool FHktVoxelVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	// 복셀 머티리얼 전용 — Default Lit 셰이더 모델만 컴파일
	return Parameters.MaterialParameters.bIsDefaultMaterial
		|| Parameters.MaterialParameters.MaterialDomain == MD_Surface;
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
