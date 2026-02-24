// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "VFXIntent.h"
#include "VFXNiagaraConfig.h"
#include "VFXGeneratorConfig.h"
#include "VFXGeneratorSubsystem.generated.h"

class FVFXLLMClient;
class FVFXTextureGenerator;
class FVFXNiagaraBuilder;
class UNiagaraSystem;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnVFXGenerationComplete, bool bSuccess, UNiagaraSystem* System);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnVFXGenerationProgress, FString PhaseName, float Progress, FString Message);

/**
 * Hkt VFX Generator 에디터 서브시스템.
 * FVFXIntent / FVFXGenerationRequest를 받아 LLM → Texture Gen → Niagara Builder 파이프라인 오케스트레이션.
 */
UCLASS()
class HKTVFXEDITOR_API UVFXGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 단일 VFX 생성 (메인 엔트리) */
	UFUNCTION(BlueprintCallable, Category = "VFX Generator")
	void GenerateVFX(const FVFXGenerationRequest& Request);

	/** 배치 생성 */
	UFUNCTION(BlueprintCallable, Category = "VFX Generator")
	void GenerateBatch(const TArray<FVFXGenerationRequest>& Requests);

	/** 퀵 생성: 자연어 설명 → Intent 추론 후 생성 */
	UFUNCTION(BlueprintCallable, Category = "VFX Generator")
	void QuickGenerate(const FString& Description);

	/** 프리셋 뱅크: EventType × Element × Intensity 조합으로 일괄 생성 */
	UFUNCTION(BlueprintCallable, Category = "VFX Generator")
	void GeneratePresetBank(
		const TArray<EVFXEventType>& EventTypes,
		const TArray<EVFXElement>& Elements,
		const TArray<float>& Intensities);

	/** 생성 완료 시 (bSuccess, 생성된 UNiagaraSystem) */
	FOnVFXGenerationComplete OnComplete;

	/** 진행 상황 (PhaseName, Progress 0~1, Message) */
	FOnVFXGenerationProgress OnProgress;

private:
	void ProcessSingleRequest(const FVFXGenerationRequest& Request);
	void OnLLMResponseReceived(bool bSuccess, const FVFXNiagaraConfig& Config, FVFXGenerationRequest OriginalRequest);
	void OnTexturesGenerated(bool bSuccess, const TMap<FString, UTexture2D*>& Textures, FVFXNiagaraConfig Config, FVFXGenerationRequest OriginalRequest);
	void ProcessNextBatchItem();
	FVFXIntent ParseQuickDescription(const FString& Description) const;

	struct FBatchState
	{
		TArray<FVFXGenerationRequest> Requests;
		TArray<UNiagaraSystem*> Results;
		int32 CurrentIndex = 0;
	};

	UPROPERTY()
	TObjectPtr<UVFXGeneratorSettings> Settings;

	TUniquePtr<FVFXLLMClient> LLMClient;
	TUniquePtr<FVFXTextureGenerator> TextureGen;
	TUniquePtr<FVFXNiagaraBuilder> NiagaraBuilder;
	TSharedPtr<FBatchState> ActiveBatch;
};
