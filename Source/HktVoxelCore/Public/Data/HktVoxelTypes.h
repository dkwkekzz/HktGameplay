// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <atomic>

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

#include "Meshing/HktVoxelVertex.h"

struct HKTVOXELCORE_API FHktVoxelChunk
{
	static constexpr int32 SIZE = 32;

	FHktVoxel Data[SIZE][SIZE][SIZE];  // ~128KB

	FIntVector ChunkCoord;             // 청크 좌표 (VM 기준)
	std::atomic<bool> bMeshDirty{true};   // 재메싱 필요 (Game↔Worker 원자적)
	std::atomic<bool> bMeshReady{false};  // 메싱 완료, GPU 업로드 대기
	std::atomic<uint32> MeshGeneration{0}; // 메싱 세대 — dirty 시 증가, 메싱 시작 시 캡처하여 완료 시 비교

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

// FHktVoxelDelta는 HktCoreEvents.h에서 정의됨 (HktCore 모듈)
