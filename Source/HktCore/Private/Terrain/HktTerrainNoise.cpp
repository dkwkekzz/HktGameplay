// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "Terrain/HktTerrainNoise.h"
#include <cmath>
#include <cstring>

// ============================================================================
// 2D/3D Simplex Gradient Vectors
// ============================================================================

// 정규화된 2D 기울기 벡터 — 모든 벡터가 동일 크기 (방향 편향 방지)
static constexpr double SQRT2_INV = 0.7071067811865476;  // 1/sqrt(2)
const double FHktTerrainNoise::Grad2[12][2] = {
	{ 1, 0}, {-1, 0}, { 0, 1}, { 0,-1},
	{ SQRT2_INV, SQRT2_INV}, {-SQRT2_INV, SQRT2_INV}, { SQRT2_INV,-SQRT2_INV}, {-SQRT2_INV,-SQRT2_INV},
	{ 1, 0}, {-1, 0}, { 0, 1}, { 0,-1}
};

const double FHktTerrainNoise::Grad3[12][3] = {
	{ 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
	{ 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
	{ 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1}
};

// ============================================================================
// Simplex Constants
// ============================================================================

static constexpr double F2 = 0.5 * (1.7320508075688772 - 1.0);  // (sqrt(3)-1)/2
static constexpr double G2 = (3.0 - 1.7320508075688772) / 6.0;  // (3-sqrt(3))/6
static constexpr double F3 = 1.0 / 3.0;
static constexpr double G3 = 1.0 / 6.0;

// ============================================================================
// Construction
// ============================================================================

FHktTerrainNoise::FHktTerrainNoise(int64 Seed)
{
	BuildPermTable(Seed);
}

void FHktTerrainNoise::SetSeed(int64 NewSeed)
{
	BuildPermTable(NewSeed);
}

void FHktTerrainNoise::BuildPermTable(int64 Seed)
{
	// Fisher-Yates shuffle with LCG
	uint8 Source[256];
	for (int32 i = 0; i < 256; ++i)
	{
		Source[i] = static_cast<uint8>(i);
	}

	// LCG parameters (Knuth)
	uint64 State = static_cast<uint64>(Seed) * 6364136223846793005ULL + 1442695040888963407ULL;

	for (int32 i = 255; i > 0; --i)
	{
		State = State * 6364136223846793005ULL + 1442695040888963407ULL;
		int32 j = static_cast<int32>((State >> 33) % (i + 1));
		uint8 Tmp = Source[i];
		Source[i] = Source[j];
		Source[j] = Tmp;
	}

	for (int32 i = 0; i < 256; ++i)
	{
		Perm[i] = Source[i];
		Perm[i + 256] = Source[i];
		Perm12[i] = static_cast<int8>(Perm[i] % 12);
		Perm12[i + 256] = Perm12[i];
	}
}

int32 FHktTerrainNoise::FastFloor(double X)
{
	int32 Xi = static_cast<int32>(X);
	return (X < Xi) ? Xi - 1 : Xi;
}

// ============================================================================
// 2D Simplex Noise
// ============================================================================

double FHktTerrainNoise::Noise2D(double X, double Y) const
{
	// Skew input space
	double S = (X + Y) * F2;
	int32 I = FastFloor(X + S);
	int32 J = FastFloor(Y + S);

	double T = (I + J) * G2;
	double X0 = X - (I - T);
	double Y0 = Y - (J - T);

	// Simplex 삼각형 결정
	int32 I1, J1;
	if (X0 > Y0) { I1 = 1; J1 = 0; }
	else          { I1 = 0; J1 = 1; }

	double X1 = X0 - I1 + G2;
	double Y1 = Y0 - J1 + G2;
	double X2 = X0 - 1.0 + 2.0 * G2;
	double Y2 = Y0 - 1.0 + 2.0 * G2;

	int32 II = I & 255;
	int32 JJ = J & 255;

	// 3개 코너 기여도 계산
	double N0 = 0.0, N1 = 0.0, N2 = 0.0;

	double T0 = 0.5 - X0 * X0 - Y0 * Y0;
	if (T0 >= 0.0)
	{
		int32 Gi0 = Perm12[II + Perm[JJ]];
		T0 *= T0;
		N0 = T0 * T0 * (Grad2[Gi0][0] * X0 + Grad2[Gi0][1] * Y0);
	}

	double T1 = 0.5 - X1 * X1 - Y1 * Y1;
	if (T1 >= 0.0)
	{
		int32 Gi1 = Perm12[II + I1 + Perm[JJ + J1]];
		T1 *= T1;
		N1 = T1 * T1 * (Grad2[Gi1][0] * X1 + Grad2[Gi1][1] * Y1);
	}

	double T2 = 0.5 - X2 * X2 - Y2 * Y2;
	if (T2 >= 0.0)
	{
		int32 Gi2 = Perm12[II + 1 + Perm[JJ + 1]];
		T2 *= T2;
		N2 = T2 * T2 * (Grad2[Gi2][0] * X2 + Grad2[Gi2][1] * Y2);
	}

	// [-1, 1] 범위로 스케일
	return 70.0 * (N0 + N1 + N2);
}

// ============================================================================
// 3D Simplex Noise
// ============================================================================

double FHktTerrainNoise::Noise3D(double X, double Y, double Z) const
{
	double S = (X + Y + Z) * F3;
	int32 I = FastFloor(X + S);
	int32 J = FastFloor(Y + S);
	int32 K = FastFloor(Z + S);

	double T = (I + J + K) * G3;
	double X0 = X - (I - T);
	double Y0 = Y - (J - T);
	double Z0 = Z - (K - T);

	// 사면체 결정
	int32 I1, J1, K1, I2, J2, K2;
	if (X0 >= Y0)
	{
		if (Y0 >= Z0)      { I1=1; J1=0; K1=0; I2=1; J2=1; K2=0; }
		else if (X0 >= Z0) { I1=1; J1=0; K1=0; I2=1; J2=0; K2=1; }
		else               { I1=0; J1=0; K1=1; I2=1; J2=0; K2=1; }
	}
	else
	{
		if (Y0 < Z0)       { I1=0; J1=0; K1=1; I2=0; J2=1; K2=1; }
		else if (X0 < Z0)  { I1=0; J1=1; K1=0; I2=0; J2=1; K2=1; }
		else               { I1=0; J1=1; K1=0; I2=1; J2=1; K2=0; }
	}

	double X1 = X0 - I1 + G3;
	double Y1 = Y0 - J1 + G3;
	double Z1 = Z0 - K1 + G3;
	double X2 = X0 - I2 + 2.0 * G3;
	double Y2 = Y0 - J2 + 2.0 * G3;
	double Z2 = Z0 - K2 + 2.0 * G3;
	double X3 = X0 - 1.0 + 3.0 * G3;
	double Y3 = Y0 - 1.0 + 3.0 * G3;
	double Z3 = Z0 - 1.0 + 3.0 * G3;

	int32 II = I & 255;
	int32 JJ = J & 255;
	int32 KK = K & 255;

	double N0 = 0.0, N1 = 0.0, N2 = 0.0, N3 = 0.0;

	double T0 = 0.6 - X0*X0 - Y0*Y0 - Z0*Z0;
	if (T0 >= 0.0)
	{
		int32 Gi = Perm12[II + Perm[JJ + Perm[KK]]];
		T0 *= T0;
		N0 = T0 * T0 * (Grad3[Gi][0]*X0 + Grad3[Gi][1]*Y0 + Grad3[Gi][2]*Z0);
	}

	double T1 = 0.6 - X1*X1 - Y1*Y1 - Z1*Z1;
	if (T1 >= 0.0)
	{
		int32 Gi = Perm12[II+I1 + Perm[JJ+J1 + Perm[KK+K1]]];
		T1 *= T1;
		N1 = T1 * T1 * (Grad3[Gi][0]*X1 + Grad3[Gi][1]*Y1 + Grad3[Gi][2]*Z1);
	}

	double T2 = 0.6 - X2*X2 - Y2*Y2 - Z2*Z2;
	if (T2 >= 0.0)
	{
		int32 Gi = Perm12[II+I2 + Perm[JJ+J2 + Perm[KK+K2]]];
		T2 *= T2;
		N2 = T2 * T2 * (Grad3[Gi][0]*X2 + Grad3[Gi][1]*Y2 + Grad3[Gi][2]*Z2);
	}

	double T3 = 0.6 - X3*X3 - Y3*Y3 - Z3*Z3;
	if (T3 >= 0.0)
	{
		int32 Gi = Perm12[II+1 + Perm[JJ+1 + Perm[KK+1]]];
		T3 *= T3;
		N3 = T3 * T3 * (Grad3[Gi][0]*X3 + Grad3[Gi][1]*Y3 + Grad3[Gi][2]*Z3);
	}

	return 32.0 * (N0 + N1 + N2 + N3);
}

// ============================================================================
// Fractal Brownian Motion
// ============================================================================

double FHktTerrainNoise::FBM2D(double X, double Y, int32 Octaves, double Lacunarity, double Persistence) const
{
	if (Octaves < 1) Octaves = 1;
	if (Octaves > 8) Octaves = 8;

	double Sum = 0.0;
	double Amplitude = 1.0;
	double Frequency = 1.0;
	double MaxAmplitude = 0.0;

	for (int32 i = 0; i < Octaves; ++i)
	{
		Sum += Noise2D(X * Frequency, Y * Frequency) * Amplitude;
		MaxAmplitude += Amplitude;
		Amplitude *= Persistence;
		Frequency *= Lacunarity;
	}

	return Sum / MaxAmplitude;
}

double FHktTerrainNoise::FBM3D(double X, double Y, double Z, int32 Octaves, double Lacunarity, double Persistence) const
{
	if (Octaves < 1) Octaves = 1;
	if (Octaves > 8) Octaves = 8;

	double Sum = 0.0;
	double Amplitude = 1.0;
	double Frequency = 1.0;
	double MaxAmplitude = 0.0;

	for (int32 i = 0; i < Octaves; ++i)
	{
		Sum += Noise3D(X * Frequency, Y * Frequency, Z * Frequency) * Amplitude;
		MaxAmplitude += Amplitude;
		Amplitude *= Persistence;
		Frequency *= Lacunarity;
	}

	return Sum / MaxAmplitude;
}

double FHktTerrainNoise::RidgedMulti2D(double X, double Y, int32 Octaves, double Lacunarity, double Persistence) const
{
	if (Octaves < 1) Octaves = 1;
	if (Octaves > 8) Octaves = 8;

	double Sum = 0.0;
	double Amplitude = 1.0;
	double Frequency = 1.0;
	double Weight = 1.0;
	double MaxAmplitude = 0.0;

	for (int32 i = 0; i < Octaves; ++i)
	{
		double Signal = Noise2D(X * Frequency, Y * Frequency);
		Signal = 1.0 - (Signal < 0.0 ? -Signal : Signal);  // abs → 반전
		Signal *= Signal;
		Signal *= Weight;
		Weight = Signal * 2.0;
		if (Weight > 1.0) Weight = 1.0;
		if (Weight < 0.0) Weight = 0.0;

		Sum += Signal * Amplitude;
		MaxAmplitude += Amplitude;
		Amplitude *= Persistence;
		Frequency *= Lacunarity;
	}

	return Sum / MaxAmplitude;
}
