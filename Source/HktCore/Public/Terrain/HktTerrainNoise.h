// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

/**
 * FHktTerrainNoise
 *
 * OpenSimplex2 기반 노이즈 생성기 (순수 C++, UE5 의존 없음).
 * 결정론적 — 동일 시드 + 좌표 = 동일 결과. 멀티플레이어 동기화 보장.
 *
 * 용도:
 *   - 하이트맵 생성 (2D)
 *   - 바이옴 온도/습도 맵 (2D)
 *   - 동굴 카빙 (3D)
 *   - 광물 분포 (3D)
 */
class HKTCORE_API FHktTerrainNoise
{
public:
	explicit FHktTerrainNoise(int64 Seed = 0);

	/** 시드 변경 (내부 순열 테이블 재생성) */
	void SetSeed(int64 NewSeed);

	/** 2D Simplex Noise — 범위 [-1, 1] */
	double Noise2D(double X, double Y) const;

	/** 3D Simplex Noise — 범위 [-1, 1] */
	double Noise3D(double X, double Y, double Z) const;

	/**
	 * 프랙탈 브라우니안 모션 (FBM) — 옥타브 중첩
	 * @param X, Y          좌표
	 * @param Octaves       옥타브 수 (1~8)
	 * @param Lacunarity    주파수 배율 (기본 2.0)
	 * @param Persistence   진폭 감쇠 (기본 0.5)
	 * @return              범위 대략 [-1, 1]
	 */
	double FBM2D(double X, double Y, int32 Octaves, double Lacunarity = 2.0, double Persistence = 0.5) const;

	/** 3D FBM */
	double FBM3D(double X, double Y, double Z, int32 Octaves, double Lacunarity = 2.0, double Persistence = 0.5) const;

	/**
	 * 리지드 멀티프랙탈 — 산악 지형에 적합한 뾰족한 능선
	 * @return  범위 [0, 1]
	 */
	double RidgedMulti2D(double X, double Y, int32 Octaves, double Lacunarity = 2.0, double Persistence = 0.5) const;

private:
	/** 순열 테이블 (512개 — 256개 × 2배 래핑) */
	uint8 Perm[512];
	int8 Perm12[512];

	void BuildPermTable(int64 Seed);

	// Simplex 기울기 벡터
	static const double Grad2[12][2];
	static const double Grad3[12][3];

	// 내부 헬퍼
	static int32 FastFloor(double X);
};
