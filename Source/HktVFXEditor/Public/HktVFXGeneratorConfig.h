// Copyright Hkt Studios, Inc. All Rights Reserved.
// HktVFXGeneratorConfig.h
// API 키 및 서비스 설정 관리
//
// API 키 저장 우선순위:
//   1. 환경변수 (CI/CD, 팀 공유 시 권장)
//   2. 로컬 .ini 파일 (개인 개발 시)
//   3. 에디터 UI에서 직접 입력 (임시)
//
// 중요: API 키가 포함된 .ini 파일은 절대 버전관리에 올리지 않는다!

#pragma once

#include "CoreMinimal.h"
#include "HktVFXGeneratorConfig.generated.h"

// ============================================================================
// LLM / 이미지 생성 Provider 열거형
// ============================================================================

UENUM(BlueprintType)
enum class EHktLLMProvider : uint8
{
	Anthropic UMETA(DisplayName = "Anthropic (Claude)"),
	OpenAI    UMETA(DisplayName = "OpenAI (GPT)"),
};

UENUM(BlueprintType)
enum class EHktImageGenProvider : uint8
{
	StableDiffusionWebUI UMETA(DisplayName = "Stable Diffusion WebUI (AUTOMATIC1111)"),
	ComfyUI              UMETA(DisplayName = "ComfyUI"),
};

// ============================================================================
// API 클라이언트용 설정 구조체 (Config → 클라이언트 전달)
// ============================================================================

struct HKTVFXEDITOR_API FHktLLMSettings
{
	EHktLLMProvider Provider = EHktLLMProvider::Anthropic;
	FString APIKey;
	FString Model;
	float Temperature = 0.7f;
	int32 MaxTokens = 4096;
};

struct HKTVFXEDITOR_API FHktImageGenSettings
{
	EHktImageGenProvider Provider = EHktImageGenProvider::StableDiffusionWebUI;
	FString ServerURL;
	FString Model;
	int32 Width = 512;
	int32 Height = 512;
	int32 Steps = 30;
	float CFGScale = 7.5f;
};

// ============================================================================
// 서브시스템용 런타임 설정 (같은 .ini에서 로드)
// ============================================================================

UCLASS(config=HKTVFX)
class HKTVFXEDITOR_API UHktVFXGeneratorSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	FString DefaultOutputDirectory = TEXT("/Game/GeneratedVFX");

	UPROPERTY(Config)
	int32 DefaultTextureResolution = 512;

	UPROPERTY(Config)
	bool bSaveLLMResponseJSON = true;

	UPROPERTY(Config)
	bool bSkipTextureGeneration = false;
};

// ============================================================================
// API 키 관리를 포함한 전체 설정 (Project Settings)
// ============================================================================
UCLASS(config=HKTVFX, defaultconfig)
class HKTVFXEDITOR_API UHktVFXGeneratorConfig : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UHktVFXGeneratorConfig() = default;

    // UDeveloperSettings - 에디터 Project Settings에 자동 등록
    virtual FName GetContainerName() const override { return TEXT("Project"); }
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
    virtual FName GetSectionName() const override { return TEXT("Hkt VFX"); }
    virtual FText GetSectionText() const override { return NSLOCTEXT("HKTVFX", "Settings", "Hkt VFX"); }

    // =======================================================================
    // LLM 설정
    // =======================================================================

    UPROPERTY(Config, EditAnywhere, Category="LLM Provider",
        meta=(DisplayName="Provider"))
    EHktLLMProvider LLMProvider = EHktLLMProvider::Anthropic;

    UPROPERTY(Config, EditAnywhere, Category="LLM Provider",
        meta=(DisplayName="API Key (leave empty to use env variable)",
              PasswordField=true))
    FString LLMApiKey;

    UPROPERTY(Config, EditAnywhere, Category="LLM Provider")
    FString LLMModel = TEXT("claude-sonnet-4-20250514");

    UPROPERTY(Config, EditAnywhere, Category="LLM Provider",
        meta=(ClampMin="0.0", ClampMax="2.0"))
    float LLMTemperature = 0.7f;

    UPROPERTY(Config, EditAnywhere, Category="LLM Provider",
        meta=(ClampMin="1024", ClampMax="8192"))
    int32 LLMMaxTokens = 4096;

    // =======================================================================
    // 이미지 생성 설정 (Stable Diffusion)
    // =======================================================================

    UPROPERTY(Config, EditAnywhere, Category="Image Generation",
        meta=(DisplayName="Provider"))
    EHktImageGenProvider ImageGenProvider = EHktImageGenProvider::StableDiffusionWebUI;

    UPROPERTY(Config, EditAnywhere, Category="Image Generation",
        meta=(DisplayName="Server URL (local SD WebUI or ComfyUI)"))
    FString ImageGenServerURL = TEXT("http://127.0.0.1:7860");

    UPROPERTY(Config, EditAnywhere, Category="Image Generation",
        meta=(DisplayName="API Key (only for cloud services, not local SD)",
              PasswordField=true))
    FString ImageGenApiKey;

    UPROPERTY(Config, EditAnywhere, Category="Image Generation")
    FString ImageGenModel = TEXT("sd_xl_base_1.0");

    UPROPERTY(Config, EditAnywhere, Category="Image Generation",
        meta=(ClampMin="256", ClampMax="2048"))
    int32 TextureResolution = 512;

    UPROPERTY(Config, EditAnywhere, Category="Image Generation",
        meta=(ClampMin="10", ClampMax="100"))
    int32 SDSteps = 30;

    UPROPERTY(Config, EditAnywhere, Category="Image Generation",
        meta=(ClampMin="1.0", ClampMax="20.0"))
    float SDCFGScale = 7.5f;

    // =======================================================================
    // 출력 설정
    // =======================================================================

    UPROPERTY(Config, EditAnywhere, Category="Output")
    FString DefaultOutputDirectory = TEXT("/Game/GeneratedVFX");

    // =======================================================================
    // 디버그
    // =======================================================================

    UPROPERTY(Config, EditAnywhere, Category="Debug")
    bool bSkipTextureGeneration = false;

    UPROPERTY(Config, EditAnywhere, Category="Debug")
    bool bSaveLLMResponseJSON = true;

    UPROPERTY(Config, EditAnywhere, Category="Debug")
    bool bLogPrompts = false;

    // =======================================================================
    // Resolved 값 가져오기 (환경변수 폴백 포함)
    // =======================================================================

    FString GetLLMApiKey() const
    {
        if (!LLMApiKey.IsEmpty())
        {
            return LLMApiKey;
        }

        FString EnvKey;
        switch (LLMProvider)
        {
        case EHktLLMProvider::Anthropic:
            EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("ANTHROPIC_API_KEY"));
            break;
        case EHktLLMProvider::OpenAI:
            EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENAI_API_KEY"));
            break;
        }

        if (EnvKey.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("[VFXConfig] No LLM API key found! "
                "Set it in Project Settings > Plugins > AI VFX Generator, "
                "or set environment variable ANTHROPIC_API_KEY / OPENAI_API_KEY"));
        }
        return EnvKey;
    }

    FString GetImageGenApiKey() const
    {
        if (!ImageGenApiKey.IsEmpty())
        {
            return ImageGenApiKey;
        }
        return FPlatformMisc::GetEnvironmentVariable(TEXT("STABILITY_API_KEY"));
    }

    FHktLLMSettings ToLLMSettings() const
    {
        FHktLLMSettings S;
        S.Provider = LLMProvider;
        S.APIKey = GetLLMApiKey();
        S.Model = LLMModel;
        S.Temperature = LLMTemperature;
        S.MaxTokens = LLMMaxTokens;
        return S;
    }

    FHktImageGenSettings ToImageGenSettings() const
    {
        FHktImageGenSettings S;
        S.Provider = ImageGenProvider;
        S.ServerURL = ImageGenServerURL;
        S.Model = ImageGenModel;
        S.Width = TextureResolution;
        S.Height = TextureResolution;
        S.Steps = SDSteps;
        S.CFGScale = SDCFGScale;
        return S;
    }

    bool Validate(FString& OutError) const
    {
        if (GetLLMApiKey().IsEmpty())
        {
            OutError = TEXT("LLM API Key is not configured. "
                "Go to Project Settings > Plugins > AI VFX Generator");
            return false;
        }

        if (ImageGenServerURL.IsEmpty() && !bSkipTextureGeneration)
        {
            OutError = TEXT("Image generation server URL is empty. "
                "Start AUTOMATIC1111 or ComfyUI locally, "
                "or enable 'Skip Texture Generation' in settings.");
            return false;
        }

        return true;
    }
};
