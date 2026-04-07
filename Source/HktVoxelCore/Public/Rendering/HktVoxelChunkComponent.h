// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "HktVoxelChunkComponent.generated.h"

class FHktVoxelRenderCache;
struct FHktVoxelChunk;

/** 텍스처+샘플러 RHI 쌍 — 타일/머티리얼 텍스처 전달에 공용 */
struct FHktVoxelTexturePair
{
	FRHITexture* Texture = nullptr;
	FRHISamplerState* Sampler = nullptr;

	bool IsValid() const { return Texture != nullptr; }
};

/** 타일 텍스처 셋 (Texture2DArray + IndexLUT) */
struct FHktVoxelTileTextureSet
{
	FHktVoxelTexturePair TileArray;
	FHktVoxelTexturePair TileIndexLUT;

	bool IsValid() const { return TileArray.IsValid() && TileIndexLUT.IsValid(); }
};

/**
 * UHktVoxelChunkComponent
 *
 * Actor에 부착하여 청크 단위 복셀 메시를 렌더링한다.
 * 메싱 완료 시 OnMeshReady()를 호출하면 SceneProxy에 새 메시 데이터를 전달한다.
 *
 * 주의: 복셀마다 Component를 만들지 말 것 — 청크(32x32x32) 단위 1개.
 */
UCLASS(ClassGroup = (HktVoxel), meta = (BlueprintSpawnableComponent))
class HKTVOXELCORE_API UHktVoxelChunkComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UHktVoxelChunkComponent();

	/** 초기화 — 렌더 캐시와 청크 좌표 바인딩 */
	void Initialize(FHktVoxelRenderCache* Cache, const FIntVector& InChunkCoord);

	/** 메싱 완료 시 호출 → SceneProxy에 새 메시 데이터 전달 (ENQUEUE_RENDER_COMMAND) */
	void OnMeshReady();

	/** 청크 좌표 반환 */
	FIntVector GetChunkCoord() const { return ChunkCoord; }

	/** 복셀 렌더링용 머티리얼 설정 (팔레트 기반 단일 머티리얼) */
	void SetVoxelMaterial(UMaterialInterface* InMaterial);

	/** 타일 텍스처 설정 — OnMeshReady에서 Proxy에 전달 */
	void SetTileTextures(const FHktVoxelTileTextureSet& InTileTextures);

	/** 머티리얼 LUT 설정 — OnMeshReady에서 Proxy에 전달 */
	void SetMaterialLUT(const FHktVoxelTexturePair& InMaterialLUT);

	// UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override { return 1; }

private:
	FIntVector ChunkCoord = FIntVector::ZeroValue;
	FHktVoxelRenderCache* RenderCache = nullptr;

	FHktVoxelTileTextureSet CachedTileTextures;
	FHktVoxelTexturePair CachedMaterialLUT;
	bool bStyleTexturesApplied = false;
};
