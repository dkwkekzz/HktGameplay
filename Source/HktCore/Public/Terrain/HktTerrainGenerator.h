// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Terrain/HktTerrainVoxel.h"
#include "Terrain/HktTerrainNoise.h"
#include "Terrain/HktTerrainBiome.h"

/**
 * FHktTerrainGeneratorConfig
 *
 * 지형 생성 파라미터. 시드 + 지형 형태 + 수면 높이 등을 정의한다.
 */
struct HKTCORE_API FHktTerrainGeneratorConfig
{
	int64 Seed = 42;

	// 지형 형태
	double HeightScale     = 64.0;   // 최대 높이 (복셀 단위)
	double HeightOffset    = 32.0;   // 기본 해수면 높이
	double TerrainFreq     = 0.008;  // 지형 노이즈 주파수 (작을수록 완만)
	int32  TerrainOctaves  = 6;      // FBM 옥타브 수
	double Lacunarity      = 2.0;
	double Persistence     = 0.5;

	// 산악
	double MountainFreq    = 0.004;  // 산악 리지 주파수
	double MountainBlend   = 0.4;    // FBM과 Ridged 혼합 비율 (0=FBM only, 1=Ridge only)

	// 수면
	double WaterLevel      = 30.0;   // 해수면 높이 (복셀 단위)

	// 동굴
	bool   bEnableCaves    = true;
	double CaveFreq        = 0.03;   // 동굴 노이즈 주파수
	double CaveThreshold   = 0.6;    // 이 값 이상이면 동굴 공간

	// 바이옴
	double BiomeNoiseScale = 0.002;  // 바이옴 노이즈 스케일
	double MountainBiomeThreshold = 80.0;

	// 청크 크기 (HktVoxelCore와 동일)
	static constexpr int32 ChunkSize = 32;
};

/**
 * FHktTerrainGenerator
 *
 * 시드 기반 결정론적 지형 생성기.
 * 청크 좌표를 입력하면 32×32×32 FHktVoxel 배열을 채운다.
 *
 * 순수 C++ — VM(HktCore)에서 실행 가능.
 *
 * 생성 파이프라인:
 *   1. 하이트맵: FBM + RidgedMulti 혼합 → 표면 높이 결정
 *   2. 바이옴: 온도+습도 노이즈 → 바이옴 타입 결정
 *   3. 재질: 바이옴 규칙 + 높이/깊이 → TypeID 배정
 *   4. 동굴: 3D 노이즈로 내부 공간 카빙
 *   5. 수면: WaterLevel 아래 빈 공간을 Water로 채움
 */
class HKTCORE_API FHktTerrainGenerator
{
public:
	explicit FHktTerrainGenerator(const FHktTerrainGeneratorConfig& Config);

	/**
	 * 단일 청크의 복셀 데이터를 생성한다.
	 * @param ChunkX, ChunkY, ChunkZ  청크 좌표
	 * @param OutVoxels  32×32×32 = 32768개 복셀 배열 (호출자가 할당)
	 *                   인덱스 = X + Y*32 + Z*32*32
	 */
	void GenerateChunk(int32 ChunkX, int32 ChunkY, int32 ChunkZ, FHktTerrainVoxel* OutVoxels) const;

	/**
	 * 특정 월드 좌표의 표면 높이를 반환한다.
	 * @param WorldX, WorldY  월드 복셀 좌표
	 * @return  표면 높이 (복셀 단위)
	 */
	double GetSurfaceHeight(double WorldX, double WorldY) const;

	/** 설정 변경 (노이즈 재생성) */
	void Reconfigure(const FHktTerrainGeneratorConfig& NewConfig);

	const FHktTerrainGeneratorConfig& GetConfig() const { return Config; }

private:
	FHktTerrainGeneratorConfig Config;

	// 노이즈 인스턴스 (각각 다른 시드)
	FHktTerrainNoise HeightNoise;     // 하이트맵
	FHktTerrainNoise MountainNoise;   // 산악 리지
	FHktTerrainNoise CaveNoise;       // 동굴
	FHktTerrainNoise TempNoise;       // 바이옴 온도
	FHktTerrainNoise HumNoise;        // 바이옴 습도

	// 바이옴 맵
	FHktTerrainBiomeMap BiomeMap;

	/** 높이 기반 재질 결정 */
	FHktTerrainVoxel DetermineVoxel(
		double WorldX, double WorldY, double WorldZ,
		double SurfaceHeight, EHktBiomeType Biome,
		const FHktBiomeMaterialRule& Rule) const;

	/** 동굴 여부 판정 */
	bool IsCave(double WorldX, double WorldY, double WorldZ, double SurfaceHeight) const;
};
