// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HktVoxelTerrainActor.generated.h"

class FHktVoxelRenderCache;
class FHktVoxelMeshScheduler;
class FHktVoxelTerrainStreamer;
class FHktTerrainGenerator;
struct FHktTerrainGeneratorConfig;
class UHktVoxelChunkComponent;

/**
 * AHktVoxelTerrainActor
 *
 * 월드에 1개 배치하여 복셀 테레인 전체를 관리한다.
 * 테레인 전용 RenderCache + MeshScheduler를 소유하고,
 * 카메라 기반 스트리밍으로 ChunkComponent를 동적 생성/풀링한다.
 *
 * 데이터 흐름:
 *   Streamer → Generator.GenerateChunk() → RenderCache → MeshScheduler → ChunkComponent → GPU
 */
UCLASS(ClassGroup = (HktVoxel))
class HKTVOXELTERRAIN_API AHktVoxelTerrainActor : public AActor
{
	GENERATED_BODY()

public:
	AHktVoxelTerrainActor();

	// === 외부 API (VM 연동) ===

	/** VM에서 청크 데이터를 수신하여 로드 */
	void LoadTerrainChunk(const FIntVector& ChunkCoord, const struct FHktVoxel* VoxelData, int32 VoxelCount);

	/** VM에서 청크 언로드 요청 (스트리밍 아웃) */
	void UnloadTerrainChunk(const FIntVector& ChunkCoord);

	/** RenderCache 직접 접근 (테스트/디버그용) */
	FHktVoxelRenderCache* GetTerrainCache() const { return TerrainCache.Get(); }

	// === 설정 ===

	/** 카메라로부터 청크 로드/유지 거리 (UE 유닛) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Streaming")
	float ViewDistance = 204800.f;  // 약 2km

	/** 프레임당 최대 청크 로드 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Streaming", meta = (ClampMin = 1, ClampMax = 32))
	int32 MaxLoadsPerFrame = 8;

	/** 프레임당 최대 메싱 수 (MeshScheduler에 전달) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Meshing", meta = (ClampMin = 1, ClampMax = 16))
	int32 MaxMeshPerFrame = 4;

	/** 테레인 높이 범위 — Z축 청크 좌표 [MinZ, MaxZ] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Streaming")
	int32 HeightMinZ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Streaming")
	int32 HeightMaxZ = 3;

	/** 테레인 렌더링용 머티리얼 (팔레트 기반) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Rendering")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** 컴포넌트 풀 초기 크기 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Streaming", meta = (ClampMin = 16))
	int32 InitialPoolSize = 256;

	// === 지형 생성 설정 ===

	/** 지형 시드 (동일 시드 = 동일 지형) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Generation")
	int64 TerrainSeed = 42;

	/** 지형 최대 높이 (복셀 단위) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Generation", meta = (ClampMin = 8, ClampMax = 256))
	double HeightScale = 64.0;

	/** 기본 해수면 높이 오프셋 (복셀 단위) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Generation")
	double HeightOffset = 32.0;

	/** 해수면 높이 (이 높이 아래 빈 공간은 물로 채워짐) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Generation")
	double WaterLevel = 30.0;

	/** 산악 지형 혼합 비율 (0=완만한 FBM만, 1=뾰족한 리지만) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Generation", meta = (ClampMin = 0.0, ClampMax = 1.0))
	double MountainBlend = 0.4;

	/** 동굴 생성 활성화 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HktTerrain|Generation")
	bool bEnableCaves = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	/** 카메라 위치 가져오기 */
	FVector GetCameraWorldPos() const;

	/** 절차적 생성 + RenderCache 로드 + 컴포넌트 할당 */
	void GenerateAndLoadChunk(const FIntVector& ChunkCoord);

	/** 스트리밍 결과 반영 — 청크 로드/언로드 + 컴포넌트 할당 */
	void ProcessStreamingResults();

	/** 메싱 완료된 청크의 컴포넌트 갱신 */
	void ProcessMeshReadyChunks();

	/** 컴포넌트 풀 관리 */
	UHktVoxelChunkComponent* AcquireComponent();
	void ReleaseComponent(UHktVoxelChunkComponent* Comp);
	void PrewarmPool(int32 Count);

	// === 내부 상태 ===

	TUniquePtr<FHktVoxelRenderCache> TerrainCache;
	TUniquePtr<FHktVoxelMeshScheduler> TerrainMeshScheduler;
	TUniquePtr<FHktVoxelTerrainStreamer> Streamer;
	TUniquePtr<FHktTerrainGenerator> Generator;

	/** 활성 청크 → 컴포넌트 매핑 */
	TMap<FIntVector, UHktVoxelChunkComponent*> ActiveChunks;

	/** 비활성 컴포넌트 풀 (재사용) */
	TArray<UHktVoxelChunkComponent*> ComponentPool;

	/** 청크 월드 크기 (32 * VoxelSize). VoxelSize = 100 UE 유닛 기준 = 3200 */
	static constexpr float ChunkWorldSize = 32.f * 100.f;
};
