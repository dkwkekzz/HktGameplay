// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HktVFXIntent.h"
#include "HktVFXNiagaraConfig.h"
#include "HktVFXGeneratorConfig.h"
#include "HktVFXGeneratorSubsystem.generated.h"

class FHktVFXLLMClient;
class FHktVFXTextureGenerator;
class FHktVFXNiagaraBuilder;
class UNiagaraSystem;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHktVFXGenerationComplete, bool bSuccess, UNiagaraSystem* System);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnHktVFXGenerationProgress, FString PhaseName, float Progress, FString Message);

/**
 * Hkt VFX Generator 에디터 서브시스템.
 * FHktVFXIntent / FHktVFXGenerationRequest를 받아 LLM → Texture Gen → Niagara Builder 파이프라인 오케스트레이션.
 */
UCLASS()
class HKTVFXEDITOR_API UHktVFXGeneratorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 단일 VFX 생성 (메인 엔트리) */
	UFUNCTION(BlueprintCallable, Category = "VFX Generator")
	void GenerateVFX(const FHktVFXGenerationRequest& Request);

	/** 배치 생성 */
	UFUNCTION(BlueprintCallable, Category = "VFX Generator")
	void GenerateBatch(const TArray<FHktVFXGenerationRequest>& Requests);

	/** 퀵 생성: 자연어 설명 → Intent 추론 후 생성 */
	UFUNCTION(BlueprintCallable, Category = "VFX Generator")
	void QuickGenerate(const FString& Description);

	/** 프리셋 뱅크: EventType × Element × Intensity 조합으로 일괄 생성 */
	UFUNCTION(BlueprintCallable, Category = "VFX Generator")
	void GeneratePresetBank(
		const TArray<EHktVFXEventType>& EventTypes,
		const TArray<EHktVFXElement>& Elements,
		const TArray<float>& Intensities);

	/** 생성 완료 시 (bSuccess, 생성된 UNiagaraSystem) */
	FOnHktVFXGenerationComplete OnComplete;

	/** 진행 상황 (PhaseName, Progress 0~1, Message) */
	FOnHktVFXGenerationProgress OnProgress;

private:
	void ProcessSingleRequest(const FHktVFXGenerationRequest& Request);
	void OnLLMResponseReceived(bool bSuccess, const FHktVFXNiagaraConfig& Config, FHktVFXGenerationRequest OriginalRequest);
	void OnTexturesGenerated(bool bSuccess, const TMap<FString, UTexture2D*>& Textures, FHktVFXNiagaraConfig Config, FHktVFXGenerationRequest OriginalRequest);
	void ProcessNextBatchItem();
	FHktVFXIntent ParseQuickDescription(const FString& Description) const;

	struct FHktVFXBatchState
	{
		TArray<FHktVFXGenerationRequest> Requests;
		TArray<UNiagaraSystem*> Results;
		int32 CurrentIndex = 0;
	};

	UPROPERTY()
	TObjectPtr<UHktVFXGeneratorSettings> Settings;

	TSharedPtr<FHktVFXLLMClient> LLMClient;
	TSharedPtr<FHktVFXTextureGenerator> TextureGen;
	TSharedPtr<FHktVFXNiagaraBuilder> NiagaraBuilder;
	TSharedPtr<FHktVFXBatchState> ActiveBatch;
};
