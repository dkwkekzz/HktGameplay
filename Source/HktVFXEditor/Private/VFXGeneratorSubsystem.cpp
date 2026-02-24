// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "VFXGeneratorSubsystem.h"
#include "VFXGeneratorConfig.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

void UVFXGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Settings = NewObject<UVFXGeneratorSettings>(this);
    Settings->LoadConfig();

    LLMClient = MakeUnique<FVFXLLMClient>();
    TextureGen = MakeUnique<FVFXTextureGenerator>();
    NiagaraBuilder = MakeUnique<FVFXNiagaraBuilder>();

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Subsystem initialized"));
}

// Config에서 설정 로드 + 유효성 검증
static bool LoadAndValidateConfig(FVFXLLMClient* LLM, FVFXTextureGenerator* TexGen)
{
    const UVFXGeneratorConfig* Config = GetDefault<UVFXGeneratorConfig>();

    FString Error;
    if (!Config->Validate(Error))
    {
        UE_LOG(LogTemp, Error, TEXT("[VFXGenerator] %s"), *Error);
        // 에디터 알림
        FNotificationInfo Info(FText::FromString(Error));
        Info.ExpireDuration = 5.f;
        FSlateNotificationManager::Get().AddNotification(Info);
        return false;
    }

    LLM->SetSettings(Config->ToLLMSettings());
    TexGen->SetSettings(Config->ToImageGenSettings());
    return true;
}

void UVFXGeneratorSubsystem::Deinitialize()
{
    LLMClient.Reset();
    TextureGen.Reset();
    NiagaraBuilder.Reset();
    Super::Deinitialize();
}

// ============================================================================
// 단일 VFX 생성 - 메인 엔트리 포인트
// ============================================================================
void UVFXGeneratorSubsystem::GenerateVFX(const FVFXGenerationRequest& Request)
{
    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] === Starting generation: %s ==="),
        *Request.Intent.GetAssetKey());

    ProcessSingleRequest(Request);
}

void UVFXGeneratorSubsystem::ProcessSingleRequest(const FVFXGenerationRequest& Request)
{
    // Config에서 API 키 로드 + 검증
    if (!LoadAndValidateConfig(LLMClient.Get(), TextureGen.Get()))
    {
        OnComplete.Broadcast(false, nullptr);
        return;
    }

    // Phase 1: LLM에 Niagara 설정 요청
    OnProgress.Broadcast(TEXT("LLM"), 0.0f,
        FString::Printf(TEXT("Requesting AI design for: %s"),
            *Request.Intent.GetAssetKey()));

    FOnLLMResponse OnLLMDone;
    OnLLMDone.BindLambda([this, Request](bool bSuccess, const FVFXNiagaraConfig& Config)
    {
        OnLLMResponseReceived(bSuccess, Config, Request);
    });

    LLMClient->RequestNiagaraConfig(Request.Intent, OnLLMDone);
}

// ============================================================================
// Phase 1 완료 → Phase 2: 텍스처 생성
// ============================================================================
void UVFXGeneratorSubsystem::OnLLMResponseReceived(
    bool bSuccess,
    const FVFXNiagaraConfig& Config,
    FVFXGenerationRequest OriginalRequest)
{
    if (!bSuccess || !Config.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[VFXGenerator] LLM request failed"));
        OnProgress.Broadcast(TEXT("LLM"), 1.0f, TEXT("Failed - LLM did not return valid config"));
        OnComplete.Broadcast(false, nullptr);
        return;
    }

    OnProgress.Broadcast(TEXT("LLM"), 1.0f,
        FString::Printf(TEXT("AI designed '%s': %d emitters, %d textures needed"),
            *Config.SystemName, Config.Emitters.Num(), Config.TextureRequests.Num()));

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Design notes: %s"), *Config.DesignNotes);

    // 디버그: JSON 저장
    if (Settings->bSaveLLMResponseJSON)
    {
        FString SavePath = FPaths::ProjectSavedDir() / TEXT("AIVFXGenerator") /
            (OriginalRequest.Intent.GetAssetKey() + TEXT("_config.json"));
        // Config를 다시 JSON으로 직렬화하여 저장 (생략 - FJsonObjectConverter 사용)
    }

    // Phase 2: 텍스처 생성
    if (Settings->bSkipTextureGeneration || Config.TextureRequests.Num() == 0)
    {
        // 텍스처 없이 바로 빌드
        TMap<FString, UTexture2D*> EmptyTextures;
        OnTexturesGenerated(true, EmptyTextures, Config, OriginalRequest);
        return;
    }

    OnProgress.Broadcast(TEXT("Texture"), 0.0f,
        FString::Printf(TEXT("Generating %d textures via Stable Diffusion..."),
            Config.TextureRequests.Num()));

    FString TexOutputDir = OriginalRequest.OutputDirectory.IsEmpty()
        ? Settings->DefaultOutputDirectory : OriginalRequest.OutputDirectory;

    int32 TexRes = OriginalRequest.TextureResolution > 0
        ? OriginalRequest.TextureResolution : Settings->DefaultTextureResolution;

    FOnAllTexturesGenerated OnTexDone;
    OnTexDone.BindLambda([this, Config, OriginalRequest](
        bool bSuccess, const TMap<FString, UTexture2D*>& Textures)
    {
        OnTexturesGenerated(bSuccess, Textures, Config, OriginalRequest);
    });

    TextureGen->GenerateAllTextures(Config.TextureRequests, TexOutputDir, TexRes, OnTexDone);
}

// ============================================================================
// Phase 2 완료 → Phase 3: Niagara 시스템 빌드
// ============================================================================
void UVFXGeneratorSubsystem::OnTexturesGenerated(
    bool bSuccess,
    const TMap<FString, UTexture2D*>& Textures,
    FVFXNiagaraConfig Config,
    FVFXGenerationRequest OriginalRequest)
{
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VFXGenerator] Some textures failed, proceeding with defaults"));
    }

    OnProgress.Broadcast(TEXT("Texture"), 1.0f,
        FString::Printf(TEXT("Generated %d/%d textures"),
            Textures.Num(), Config.TextureRequests.Num()));

    // Phase 3: Niagara 시스템 빌드
    OnProgress.Broadcast(TEXT("Build"), 0.0f, TEXT("Building Niagara System..."));

    FString OutputDir = OriginalRequest.OutputDirectory.IsEmpty()
        ? Settings->DefaultOutputDirectory : OriginalRequest.OutputDirectory;

    UNiagaraSystem* System = NiagaraBuilder->BuildNiagaraSystem(Config, Textures, OutputDir);

    if (System)
    {
        OnProgress.Broadcast(TEXT("Build"), 1.0f,
            FString::Printf(TEXT("✓ Created: %s"), *System->GetPathName()));

        UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] === Generation complete: %s ==="),
            *System->GetPathName());
    }
    else
    {
        OnProgress.Broadcast(TEXT("Build"), 1.0f, TEXT("✗ Failed to build Niagara System"));
    }

    OnComplete.Broadcast(System != nullptr, System);

    // 배치 처리 중이면 다음 아이템
    if (ActiveBatch.IsValid())
    {
        if (System) ActiveBatch->Results.Add(System);
        ProcessNextBatchItem();
    }
}

// ============================================================================
// 배치 생성
// ============================================================================
void UVFXGeneratorSubsystem::GenerateBatch(const TArray<FVFXGenerationRequest>& Requests)
{
    if (Requests.Num() == 0) return;

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Starting batch generation: %d items"), Requests.Num());

    ActiveBatch = MakeShared<FBatchState>();
    ActiveBatch->Requests = Requests;
    ActiveBatch->CurrentIndex = 0;

    ProcessNextBatchItem();
}

void UVFXGeneratorSubsystem::ProcessNextBatchItem()
{
    if (!ActiveBatch.IsValid()) return;

    if (ActiveBatch->CurrentIndex >= ActiveBatch->Requests.Num())
    {
        UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Batch complete: %d/%d succeeded"),
            ActiveBatch->Results.Num(), ActiveBatch->Requests.Num());
        ActiveBatch.Reset();
        return;
    }

    const FVFXGenerationRequest& Request = ActiveBatch->Requests[ActiveBatch->CurrentIndex];
    ActiveBatch->CurrentIndex++;

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Batch item %d/%d: %s"),
        ActiveBatch->CurrentIndex, ActiveBatch->Requests.Num(),
        *Request.Intent.GetAssetKey());

    ProcessSingleRequest(Request);
}

// ============================================================================
// 퀵 생성 - 자연어 → Intent 변환
// ============================================================================
void UVFXGeneratorSubsystem::QuickGenerate(const FString& Description)
{
    FVFXIntent Intent = ParseQuickDescription(Description);

    FVFXGenerationRequest Request;
    Request.Intent = Intent;
    Request.OutputDirectory = Settings->DefaultOutputDirectory;
    Request.TextureResolution = Settings->DefaultTextureResolution;

    GenerateVFX(Request);
}

FVFXIntent UVFXGeneratorSubsystem::ParseQuickDescription(const FString& Description) const
{
    FVFXIntent Intent;
    FString Desc = Description.ToLower();

    // 이벤트 타입 추론
    if (Desc.Contains(TEXT("explo")))        Intent.EventType = EVFXEventType::Explosion;
    else if (Desc.Contains(TEXT("hit")))     Intent.EventType = EVFXEventType::ProjectileHit;
    else if (Desc.Contains(TEXT("trail")))   Intent.EventType = EVFXEventType::ProjectileTrail;
    else if (Desc.Contains(TEXT("area")))    Intent.EventType = EVFXEventType::AreaEffect;
    else if (Desc.Contains(TEXT("buff")))    Intent.EventType = EVFXEventType::Buff;
    else if (Desc.Contains(TEXT("heal")))    Intent.EventType = EVFXEventType::Heal;
    else if (Desc.Contains(TEXT("shield")))  Intent.EventType = EVFXEventType::Shield;
    else if (Desc.Contains(TEXT("summon")))  Intent.EventType = EVFXEventType::Summon;
    else if (Desc.Contains(TEXT("death")))   Intent.EventType = EVFXEventType::Death;
    else                                     Intent.EventType = EVFXEventType::Custom;

    // 속성 추론
    if (Desc.Contains(TEXT("fire")) || Desc.Contains(TEXT("flame")))
        Intent.Element = EVFXElement::Fire;
    else if (Desc.Contains(TEXT("ice")) || Desc.Contains(TEXT("frost")))
        Intent.Element = EVFXElement::Ice;
    else if (Desc.Contains(TEXT("lightning")) || Desc.Contains(TEXT("electric")))
        Intent.Element = EVFXElement::Lightning;
    else if (Desc.Contains(TEXT("dark")) || Desc.Contains(TEXT("shadow")))
        Intent.Element = EVFXElement::Dark;
    else if (Desc.Contains(TEXT("holy")) || Desc.Contains(TEXT("light")) || Desc.Contains(TEXT("divine")))
        Intent.Element = EVFXElement::Holy;
    else if (Desc.Contains(TEXT("poison")) || Desc.Contains(TEXT("toxic")))
        Intent.Element = EVFXElement::Poison;
    else if (Desc.Contains(TEXT("water")))
        Intent.Element = EVFXElement::Water;
    else if (Desc.Contains(TEXT("earth")) || Desc.Contains(TEXT("rock")))
        Intent.Element = EVFXElement::Earth;
    else if (Desc.Contains(TEXT("wind")))
        Intent.Element = EVFXElement::Wind;
    else if (Desc.Contains(TEXT("arcane")) || Desc.Contains(TEXT("magic")))
        Intent.Element = EVFXElement::Arcane;
    else if (Desc.Contains(TEXT("nature")) || Desc.Contains(TEXT("leaf")))
        Intent.Element = EVFXElement::Nature;

    // 강도 추론
    if (Desc.Contains(TEXT("weak")) || Desc.Contains(TEXT("small")) || Desc.Contains(TEXT("subtle")))
        Intent.Intensity = 0.3f;
    else if (Desc.Contains(TEXT("strong")) || Desc.Contains(TEXT("powerful")) || Desc.Contains(TEXT("big")))
        Intent.Intensity = 0.8f;
    else if (Desc.Contains(TEXT("massive")) || Desc.Contains(TEXT("epic")) || Desc.Contains(TEXT("huge")))
        Intent.Intensity = 1.0f;

    // 표면 추론
    if (Desc.Contains(TEXT("stone")))      Intent.SurfaceType = EVFXSurfaceType::Stone;
    else if (Desc.Contains(TEXT("metal"))) Intent.SurfaceType = EVFXSurfaceType::Metal;
    else if (Desc.Contains(TEXT("water"))) Intent.SurfaceType = EVFXSurfaceType::Water;
    else if (Desc.Contains(TEXT("snow")))  Intent.SurfaceType = EVFXSurfaceType::Snow;

    // 원본 설명도 보존 (Custom 타입이면 LLM이 그대로 활용)
    Intent.CustomDescription = Description;

    return Intent;
}

// ============================================================================
// 프리셋 뱅크 자동 생성
// ============================================================================
void UVFXGeneratorSubsystem::GeneratePresetBank(
    const TArray<EVFXEventType>& EventTypes,
    const TArray<EVFXElement>& Elements,
    const TArray<float>& Intensities)
{
    TArray<FVFXGenerationRequest> Requests;

    for (EVFXEventType Event : EventTypes)
    {
        for (EVFXElement Element : Elements)
        {
            for (float Intensity : Intensities)
            {
                FVFXGenerationRequest Req;
                Req.Intent.EventType = Event;
                Req.Intent.Element = Element;
                Req.Intent.Intensity = Intensity;
                Req.OutputDirectory = Settings->DefaultOutputDirectory /
                    UEnum::GetValueAsString(Event).RightChop(16);
                Req.TextureResolution = Settings->DefaultTextureResolution;
                Requests.Add(Req);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Preset bank: %d combinations to generate"), Requests.Num());
    GenerateBatch(Requests);
}