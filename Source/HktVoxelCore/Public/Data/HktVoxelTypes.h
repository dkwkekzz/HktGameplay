// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ============================================================================
// FHktVoxel — 단일 복셀 데이터 (렌더링 전용)
// ============================================================================

struct FHktVoxel
{
	uint16 TypeID = 0;        // 복셀 종류 (0 = 빈 공간)
	uint8  PaletteIndex = 0;  // 8색 팔레트 내 인덱스
	uint8  Flags = 0;         // 비트플래그: 투명, 발광, 파괴가능 등

	// 플래그 비트 정의
	static constexpr uint8 FLAG_TRANSLUCENT  = 0x01;
	static constexpr uint8 FLAG_EMISSIVE     = 0x02;
	static constexpr uint8 FLAG_DESTRUCTIBLE = 0x04;
	static constexpr uint8 FLAG_ANIMATED     = 0x08;

	bool IsEmpty() const { return TypeID == 0; }
	bool IsTranslucent() const { return (Flags & FLAG_TRANSLUCENT) != 0; }
	bool IsEmissive() const { return (Flags & FLAG_EMISSIVE) != 0; }
};

// ============================================================================
// FHktVoxelChunk — 32x32x32 복셀 청크 (렌더링 전용 사본)
// ============================================================================

struct FHktVoxelVertex;  // Forward declaration — Meshing/HktVoxelVertex.h 참조

struct HKTVOXELCORE_API FHktVoxelChunk
{
	static constexpr int32 SIZE = 32;

	FHktVoxel Data[SIZE][SIZE][SIZE];  // ~128KB

	FIntVector ChunkCoord;             // 청크 좌표 (VM 기준)
	bool bMeshDirty = true;            // 재메싱 필요
	bool bMeshReady = false;           // 메싱 완료, GPU 업로드 대기

	// Greedy Meshing 결과 — MeshChunk()가 채움
	TArray<FHktVoxelVertex> OpaqueVertices;
	TArray<uint32> OpaqueIndices;
	TArray<FHktVoxelVertex> TranslucentVertices;
	TArray<uint32> TranslucentIndices;

	// 복셀 접근 (로컬 좌표)
	FHktVoxel& At(int32 X, int32 Y, int32 Z) { return Data[X][Y][Z]; }
	const FHktVoxel& At(int32 X, int32 Y, int32 Z) const { return Data[X][Y][Z]; }

	// LocalIndex (0~32767) ↔ 로컬 좌표 변환
	static FIntVector IndexToLocal(uint16 LocalIndex)
	{
		return FIntVector(
			LocalIndex % SIZE,
			(LocalIndex / SIZE) % SIZE,
			LocalIndex / (SIZE * SIZE)
		);
	}

	static uint16 LocalToIndex(int32 X, int32 Y, int32 Z)
	{
		return static_cast<uint16>(X + Y * SIZE + Z * SIZE * SIZE);
	}
};

// ============================================================================
// FHktVoxelDelta — VM → 렌더 캐시 복셀 변경 이벤트
// ============================================================================

struct HKTVOXELCORE_API FHktVoxelDelta
{
	FIntVector ChunkCoord;      // 대상 청크
	uint16 LocalIndex = 0;      // 청크 내 복셀 인덱스 (0~32767)
	uint16 NewTypeID = 0;
	uint8  NewPaletteIndex = 0;
	uint8  NewFlags = 0;

	friend FArchive& operator<<(FArchive& Ar, FHktVoxelDelta& D)
	{
		Ar << D.ChunkCoord << D.LocalIndex << D.NewTypeID << D.NewPaletteIndex << D.NewFlags;
		return Ar;
	}
};
