// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Settings/HktRuntimeGlobalSetting.h"

UHktRuntimeGlobalSetting::UHktRuntimeGlobalSetting()
{
}

FHktTerrainGeneratorConfig UHktRuntimeGlobalSetting::ToTerrainConfig() const
{
	FHktTerrainGeneratorConfig Config;
	Config.Seed                    = TerrainSeed;
	Config.HeightScale             = HeightScale;
	Config.HeightOffset            = HeightOffset;
	Config.TerrainFreq             = TerrainFreq;
	Config.TerrainOctaves          = TerrainOctaves;
	Config.Lacunarity              = Lacunarity;
	Config.Persistence             = Persistence;
	Config.MountainFreq            = MountainFreq;
	Config.MountainBlend           = MountainBlend;
	Config.WaterLevel              = WaterLevel;
	Config.bEnableCaves            = bEnableCaves;
	Config.CaveFreq                = CaveFreq;
	Config.CaveThreshold           = CaveThreshold;
	Config.BiomeNoiseScale         = BiomeNoiseScale;
	Config.MountainBiomeThreshold  = MountainBiomeThreshold;
	return Config;
}

FVector UHktRuntimeGlobalSetting::ComputeDefaultSpawnLocation() const
{
	const FHktTerrainGeneratorConfig Config = ToTerrainConfig();
	const FHktTerrainGenerator Generator(Config);

	const double VoxelX = DefaultSpawnVoxelXY.X;
	const double VoxelY = DefaultSpawnVoxelXY.Y;
	const double SurfaceZ = Generator.GetSurfaceHeight(VoxelX, VoxelY);

	// 복셀 → cm 변환 (복셀 중심 = voxel * 15 + 7.5)
	constexpr double VoxelSizeCm = 15.0;
	constexpr double Half = VoxelSizeCm * 0.5;
	return FVector(
		VoxelX * VoxelSizeCm + Half,
		VoxelY * VoxelSizeCm + Half,
		(SurfaceZ + 1.0) * VoxelSizeCm + Half);  // +1: 표면 위 1복셀
}
