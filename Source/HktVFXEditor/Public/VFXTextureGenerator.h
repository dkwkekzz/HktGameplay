// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VFXNiagaraConfig.h"
#include "VFXGeneratorConfig.h"

class UTexture2D;

/**
 * Stable Diffusion WebUI / ComfyUI API 호출 → 파티클용 텍스처 에셋 생성.
 * 에디터 전용.
 */
class HKTVFXEDITOR_API FVFXTextureGenerator
{
public:
	/** 설정 적용 (Config에서 ToImageGenSettings()로 전달) */
	void SetSettings(const FImageGenSettings& InSettings);

	/** 단일 텍스처 생성 (비동기 콜백) */
	void GenerateTexture(
		const FVFXTextureRequest& Request,
		const FString& OutputPath,
		int32 Resolution,
		FOnTextureGenerated OnComplete);

	/** 배치 텍스처 생성 (EmitterName → UTexture2D* 맵으로 콜백) */
	void GenerateAllTextures(
		const TArray<FVFXTextureRequest>& Requests,
		const FString& OutputDirectory,
		int32 Resolution,
		FOnAllTexturesGenerated OnComplete);

private:
	FString BuildFinalPrompt(const FVFXTextureRequest& Request) const;
	FString BuildFinalNegativePrompt(const FVFXTextureRequest& Request) const;
	void CallSD_WebUI(const FString& Prompt, const FString& NegativePrompt,
		int32 Width, int32 Height,
		TFunction<void(bool, const TArray<uint8>&)> OnComplete);
	void CallComfyUI(const FString& Prompt, const FString& NegativePrompt,
		int32 Width, int32 Height,
		TFunction<void(bool, const TArray<uint8>&)> OnComplete);
	void PollComfyUIResult(const FString& PromptId, TFunction<void(bool, const TArray<uint8>&)> OnComplete);
	UTexture2D* ImportTextureFromPNG(const TArray<uint8>& PNGData, const FString& AssetPath, const FString& TextureName);
	void ConfigureTextureForVFX(UTexture2D* Texture, const FString& BlendMode);

	struct FPendingBatch
	{
		TArray<FVFXTextureRequest> Requests;
		FString OutputDirectory;
		int32 Resolution = 512;
		int32 CurrentIndex = 0;
		TMap<FString, UTexture2D*> Results;
		FOnAllTexturesGenerated OnComplete;
	};
	void ProcessNextInBatch(TSharedPtr<FPendingBatch> Batch);

	FImageGenSettings Settings;
};
