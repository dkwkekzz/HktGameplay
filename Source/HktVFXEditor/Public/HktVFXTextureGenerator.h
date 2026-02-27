// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktVFXNiagaraConfig.h"
#include "HktVFXGeneratorConfig.h"

class UTexture2D;

/**
 * Stable Diffusion WebUI / ComfyUI API 호출 → 파티클용 텍스처 에셋 생성.
 * 에디터 전용.
 */
class HKTVFXEDITOR_API FHktVFXTextureGenerator
{
public:
	/** 설정 적용 (Config에서 ToImageGenSettings()로 전달) */
	void SetSettings(const FHktImageGenSettings& InSettings);

	/** 단일 텍스처 생성 (비동기 콜백) */
	void GenerateTexture(
		const FHktVFXTextureRequest& Request,
		const FString& OutputPath,
		int32 Resolution,
		FOnHktTextureGenerated OnComplete);

	/** 배치 텍스처 생성 (EmitterName → UTexture2D* 맵으로 콜백) */
	void GenerateAllTextures(
		const TArray<FHktVFXTextureRequest>& Requests,
		const FString& OutputDirectory,
		int32 Resolution,
		FOnHktAllTexturesGenerated OnComplete);

private:
	FString BuildFinalPrompt(const FHktVFXTextureRequest& Request) const;
	FString BuildFinalNegativePrompt(const FHktVFXTextureRequest& Request) const;
	void CallSD_WebUI(const FString& Prompt, const FString& NegativePrompt,
		int32 Width, int32 Height,
		TFunction<void(bool, const TArray<uint8>&)> OnComplete);
	void CallComfyUI(const FString& Prompt, const FString& NegativePrompt,
		int32 Width, int32 Height,
		TFunction<void(bool, const TArray<uint8>&)> OnComplete);
	void PollComfyUIResult(const FString& PromptId, TFunction<void(bool, const TArray<uint8>&)> OnComplete);
	UTexture2D* ImportTextureFromPNG(const TArray<uint8>& PNGData, const FString& AssetPath, const FString& TextureName);
	void ConfigureTextureForVFX(UTexture2D* Texture, const FString& BlendMode);

	struct FHktVFXPendingBatch
	{
		TArray<FHktVFXTextureRequest> Requests;
		FString OutputDirectory;
		int32 Resolution = 512;
		int32 CurrentIndex = 0;
		TMap<FString, UTexture2D*> Results;
		FOnHktAllTexturesGenerated OnComplete;
	};
	void ProcessNextInBatch(TSharedPtr<FHktVFXPendingBatch> Batch);

	FHktImageGenSettings Settings;
};
