// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Terrain/HktTerrainGenerator.h"

namespace
{
	constexpr uint16 TYPE_AIR   = 0;
	constexpr uint16 TYPE_WATER = 5;
	constexpr uint16 TYPE_ICE   = 7;
}

FHktTerrainGenerator::FHktTerrainGenerator(const FHktTerrainGeneratorConfig& InConfig)
	: Config(InConfig)
	, HeightNoise(InConfig.Seed)
	, MountainNoise(InConfig.Seed + 1)
	, CaveNoise(InConfig.Seed + 2)
	, TempNoise(InConfig.Seed + 3)
	, HumNoise(InConfig.Seed + 4)
	, BiomeMap(&TempNoise, &HumNoise)
{
	BiomeMap.SetNoiseScale(Config.BiomeNoiseScale);
	BiomeMap.SetMountainThreshold(Config.MountainBiomeThreshold);
}

void FHktTerrainGenerator::Reconfigure(const FHktTerrainGeneratorConfig& NewConfig)
{
	Config = NewConfig;
	HeightNoise.SetSeed(Config.Seed);
	MountainNoise.SetSeed(Config.Seed + 1);
	CaveNoise.SetSeed(Config.Seed + 2);
	TempNoise.SetSeed(Config.Seed + 3);
	HumNoise.SetSeed(Config.Seed + 4);
	BiomeMap.SetNoiseScale(Config.BiomeNoiseScale);
	BiomeMap.SetMountainThreshold(Config.MountainBiomeThreshold);
}

double FHktTerrainGenerator::GetSurfaceHeight(double WorldX, double WorldY) const
{
	double FBMHeight = HeightNoise.FBM2D(
		WorldX * Config.TerrainFreq,
		WorldY * Config.TerrainFreq,
		Config.TerrainOctaves,
		Config.Lacunarity,
		Config.Persistence);

	double RidgeHeight = MountainNoise.RidgedMulti2D(
		WorldX * Config.MountainFreq,
		WorldY * Config.MountainFreq,
		Config.TerrainOctaves,
		Config.Lacunarity,
		Config.Persistence);

	double Blend = Config.MountainBlend;
	double Mixed = (1.0 - Blend) * FBMHeight + Blend * RidgeHeight;

	return Mixed * Config.HeightScale + Config.HeightOffset;
}

bool FHktTerrainGenerator::IsCave(double WorldX, double WorldY, double WorldZ, double SurfaceHeight) const
{
	if (!Config.bEnableCaves)
	{
		return false;
	}

	// 표면 근처 (3블록 이내)에서는 동굴 생성 안 함
	if (WorldZ >= SurfaceHeight - 3.0)
	{
		return false;
	}

	double CaveValue = CaveNoise.FBM3D(
		WorldX * Config.CaveFreq,
		WorldY * Config.CaveFreq,
		WorldZ * Config.CaveFreq,
		3, 2.0, 0.5);

	// 절대값이 작을수록 동굴 (스파게티 동굴)
	double AbsCave = (CaveValue < 0.0) ? -CaveValue : CaveValue;
	return AbsCave < (1.0 - Config.CaveThreshold);
}

FHktTerrainVoxel FHktTerrainGenerator::DetermineVoxel(
	double WorldX, double WorldY, double WorldZ,
	double SurfaceHeight, EHktBiomeType Biome,
	const FHktBiomeMaterialRule& Rule) const
{
	FHktTerrainVoxel Voxel;

	double Depth = SurfaceHeight - WorldZ;

	if (WorldZ > SurfaceHeight)
	{
		// 표면 위: 공기 또는 수면
		if (WorldZ <= Config.WaterLevel && Rule.WaterType != TYPE_AIR)
		{
			Voxel.TypeID = Rule.WaterType;
			if (Rule.WaterType == TYPE_WATER || Rule.WaterType == TYPE_ICE)
			{
				Voxel.Flags = FHktTerrainVoxel::FLAG_TRANSLUCENT;
			}
		}
		return Voxel;
	}

	// 표면 이하: 깊이 기반 재질 결정
	if (Depth < 1.0)
	{
		Voxel.TypeID = Rule.SurfaceType;
		Voxel.PaletteIndex = 0;
	}
	else if (Depth < 4.0)
	{
		Voxel.TypeID = Rule.SubsurfaceType;
		Voxel.PaletteIndex = 1;
	}
	else if (WorldZ <= 2.0)
	{
		Voxel.TypeID = Rule.BedrockType;
		Voxel.PaletteIndex = 3;
	}
	else
	{
		Voxel.TypeID = Rule.DeepType;
		Voxel.PaletteIndex = 2;
	}

	return Voxel;
}

void FHktTerrainGenerator::GenerateChunk(int32 ChunkX, int32 ChunkY, int32 ChunkZ, FHktTerrainVoxel* OutVoxels) const
{
	constexpr int32 S = FHktTerrainGeneratorConfig::ChunkSize;

	const double BaseX = static_cast<double>(ChunkX) * S;
	const double BaseY = static_cast<double>(ChunkY) * S;
	const double BaseZ = static_cast<double>(ChunkZ) * S;

	for (int32 Z = 0; Z < S; ++Z)
	{
		const double WorldZ = BaseZ + Z;

		for (int32 Y = 0; Y < S; ++Y)
		{
			const double WorldY = BaseY + Y;

			for (int32 X = 0; X < S; ++X)
			{
				const double WorldX = BaseX + X;
				const int32 Index = X + Y * S + Z * S * S;

				// 1. 표면 높이
				double SurfaceH = GetSurfaceHeight(WorldX, WorldY);

				// 2. 바이옴
				EHktBiomeType Biome = BiomeMap.GetBiomeWithHeight(WorldX, WorldY, SurfaceH);
				const FHktBiomeMaterialRule& Rule = BiomeMap.GetMaterialRule(Biome);

				// 3. 재질
				FHktTerrainVoxel Voxel = DetermineVoxel(WorldX, WorldY, WorldZ, SurfaceH, Biome, Rule);

				// 4. 동굴 카빙
				if (Voxel.TypeID != TYPE_AIR && IsCave(WorldX, WorldY, WorldZ, SurfaceH))
				{
					if (WorldZ <= Config.WaterLevel && Rule.WaterType != TYPE_AIR)
					{
						Voxel.TypeID = Rule.WaterType;
						Voxel.PaletteIndex = 0;
						Voxel.Flags = FHktTerrainVoxel::FLAG_TRANSLUCENT;
					}
					else
					{
						Voxel = FHktTerrainVoxel();
					}
				}

				OutVoxels[Index] = Voxel;
			}
		}
	}
}
