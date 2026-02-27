// Copyright Hkt Studios, Inc. All Rights Reserved.
// LLM JSON 응답 및 Niagara 빌드용 설정 구조체·델리게이트

#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class UTexture2D;
struct FHktVFXNiagaraConfig;

// ============================================================================
// 델리게이트
// ============================================================================

using FHktTextureMap = TMap<FString, UTexture2D*>;

DECLARE_DELEGATE_TwoParams(FOnHktLLMResponse, bool /*bSuccess*/, const FHktVFXNiagaraConfig& /*Config*/);
DECLARE_DELEGATE_TwoParams(FOnHktAllTexturesGenerated, bool /*bSuccess*/, const FHktTextureMap& /*Textures*/);
DECLARE_DELEGATE_TwoParams(FOnHktTextureGenerated, bool /*bSuccess*/, UTexture2D* /*Texture*/);

// ============================================================================
// 커브 포인트
// ============================================================================

struct HKTVFXEDITOR_API FHktVFXCurvePoint
{
	float Time = 0.f;
	float Value = 0.f;
};

struct HKTVFXEDITOR_API FHktVFXColorCurvePoint
{
	float Time = 0.f;
	FLinearColor Color = FLinearColor::White;
};

// ============================================================================
// 에미터 Spawn / Init / Update / Render
// ============================================================================

struct HKTVFXEDITOR_API FHktVFXEmitterSpawnConfig
{
	FString Mode = TEXT("burst");
	float Rate = 0.f;
	int32 BurstCount = 0;
	float BurstDelay = 0.f;
};

struct HKTVFXEDITOR_API FHktVFXEmitterInitConfig
{
	float LifetimeMin = 0.5f;
	float LifetimeMax = 1.0f;
	float SizeMin = 10.f;
	float SizeMax = 50.f;
	FVector VelocityMin = FVector::ZeroVector;
	FVector VelocityMax = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	FLinearColor ColorVariation = FLinearColor::Black;
};

struct HKTVFXEDITOR_API FHktVFXEmitterUpdateConfig
{
	FVector Gravity = FVector(0.f, 0.f, -980.f);
	float Drag = 0.f;
	TArray<FHktVFXCurvePoint> SizeCurve;
	TArray<FHktVFXColorCurvePoint> ColorCurve;
	TArray<FHktVFXCurvePoint> OpacityCurve;
	float RotationRateMin = 0.f;
	float RotationRateMax = 0.f;
	bool bOrbitCenter = false;
	float OrbitRadius = 0.f;
	float OrbitSpeed = 0.f;
	bool bAttractToCenter = false;
	float AttractionStrength = 0.f;
	float NoiseStrength = 0.f;
	float NoiseFrequency = 1.f;
};

struct HKTVFXEDITOR_API FHktVFXEmitterRenderConfig
{
	FString RendererType = TEXT("sprite");
	FString BlendMode = TEXT("additive");
	FString TextureStyle = TEXT("soft_circle");
	int32 SortOrder = 0;
	FString Alignment = TEXT("camera_facing");
	int32 SubUVFrames = 1;
	float RibbonWidth = 10.f;
	float LightRadius = 200.f;
	float LightIntensity = 5.f;
};

// ============================================================================
// 단일 에미터 설정
// ============================================================================

struct HKTVFXEDITOR_API FHktVFXEmitterConfig
{
	FString Name;
	FString Purpose;
	FHktVFXEmitterSpawnConfig Spawn;
	FHktVFXEmitterInitConfig Init;
	FHktVFXEmitterUpdateConfig Update;
	FHktVFXEmitterRenderConfig Render;
};

// ============================================================================
// 텍스처 생성 요청 (LLM이 지정)
// ============================================================================

struct HKTVFXEDITOR_API FHktVFXTextureRequest
{
	FString EmitterName;
	FString Prompt;
	FString NegativePrompt;
	FString TextureType = TEXT("diffuse");
	int32 FlipbookGridSize = 1;
};

// ============================================================================
// Niagara 시스템 전체 설정 (LLM JSON → 파싱 결과)
// ============================================================================

struct HKTVFXEDITOR_API FHktVFXNiagaraConfig
{
	FString SystemName;
	FString DesignNotes;
	TArray<FHktVFXEmitterConfig> Emitters;
	TArray<FHktVFXTextureRequest> TextureRequests;
	TArray<FString> ExposedParameters;

	bool IsValid() const { return Emitters.Num() > 0; }
};
