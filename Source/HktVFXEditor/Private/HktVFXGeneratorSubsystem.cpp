// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXGeneratorSubsystem.h"
#include "HktVFXGeneratorConfig.h"
#include "HktVFXLLMClient.h"
#include "HktVFXTextureGenerator.h"
#include "HktVFXNiagaraBuilder.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "NiagaraSystem.h"

void UHktVFXGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Settings = NewObject<UHktVFXGeneratorSettings>(this);
    Settings->LoadConfig();

    LLMClient = MakeShared<FHktVFXLLMClient>();
    TextureGen = MakeShared<FHktVFXTextureGenerator>();
    NiagaraBuilder = MakeShared<FHktVFXNiagaraBuilder>();

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Subsystem initialized"));
}

// Config에서 설정 로드 + 유효성 검증
static bool LoadAndValidateConfig(FHktVFXLLMClient* LLM, FHktVFXTextureGenerator* TexGen)
{
    const UHktVFXGeneratorConfig* Config = GetDefault<UHktVFXGeneratorConfig>();

    FString Error;
    if (!Config->Validate(Error))
    {
        UE_LOG(LogTemp, Error, TEXT("[VFXGenerator] %s"), *Error);
        FNotificationInfo Info(FText::FromString(Error));
        Info.ExpireDuration = 5.f;
        FSlateNotificationManager::Get().AddNotification(Info);
        return false;
    }

    LLM->SetSettings(Config->ToLLMSettings());
    TexGen->SetSettings(Config->ToImageGenSettings());
    return true;
}

void UHktVFXGeneratorSubsystem::Deinitialize()
{
    LLMClient.Reset();
    TextureGen.Reset();
    NiagaraBuilder.Reset();
    Super::Deinitialize();
}

void UHktVFXGeneratorSubsystem::GenerateVFX(const FHktVFXGenerationRequest& Request)
{
    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] === Starting generation: %s ==="),
        *Request.Intent.GetAssetKey());

    ProcessSingleRequest(Request);
}

void UHktVFXGeneratorSubsystem::ProcessSingleRequest(const FHktVFXGenerationRequest& Request)
{
    if (!LoadAndValidateConfig(LLMClient.Get(), TextureGen.Get()))
    {
        OnComplete.Broadcast(false, nullptr);
        return;
    }

    OnProgress.Broadcast(TEXT("LLM"), 0.0f,
        FString::Printf(TEXT("Requesting AI design for: %s"),
            *Request.Intent.GetAssetKey()));

    FOnHktLLMResponse OnLLMDone;
    OnLLMDone.BindLambda([this, Request](bool bSuccess, const FHktVFXNiagaraConfig& Config)
    {
        OnLLMResponseReceived(bSuccess, Config, Request);
    });

    LLMClient->RequestNiagaraConfig(Request.Intent, OnLLMDone);
}

void UHktVFXGeneratorSubsystem::OnLLMResponseReceived(
    bool bSuccess,
    const FHktVFXNiagaraConfig& Config,
    FHktVFXGenerationRequest OriginalRequest)
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

    if (Settings->bSaveLLMResponseJSON)
    {
        FString SavePath = FPaths::ProjectSavedDir() / TEXT("AIVFXGenerator") /
            (OriginalRequest.Intent.GetAssetKey() + TEXT("_config.json"));
    }

    if (Settings->bSkipTextureGeneration || Config.TextureRequests.Num() == 0)
    {
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

    FOnHktAllTexturesGenerated OnTexDone;
    OnTexDone.BindLambda([this, Config, OriginalRequest](
        bool bSuccess, const TMap<FString, UTexture2D*>& Textures)
    {
        OnTexturesGenerated(bSuccess, Textures, Config, OriginalRequest);
    });

    TextureGen->GenerateAllTextures(Config.TextureRequests, TexOutputDir, TexRes, OnTexDone);
}

void UHktVFXGeneratorSubsystem::OnTexturesGenerated(
    bool bSuccess,
    const TMap<FString, UTexture2D*>& Textures,
    FHktVFXNiagaraConfig Config,
    FHktVFXGenerationRequest OriginalRequest)
{
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VFXGenerator] Some textures failed, proceeding with defaults"));
    }

    OnProgress.Broadcast(TEXT("Texture"), 1.0f,
        FString::Printf(TEXT("Generated %d/%d textures"),
            Textures.Num(), Config.TextureRequests.Num()));

    OnProgress.Broadcast(TEXT("Build"), 0.0f, TEXT("Building Niagara System..."));

    FString OutputDir = OriginalRequest.OutputDirectory.IsEmpty()
        ? Settings->DefaultOutputDirectory : OriginalRequest.OutputDirectory;

    UNiagaraSystem* System = NiagaraBuilder->BuildNiagaraSystem(Config, Textures, OutputDir);

    if (System)
    {
        OnProgress.Broadcast(TEXT("Build"), 1.0f,
            FString::Printf(TEXT("Created: %s"), *System->GetPathName()));

        UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] === Generation complete: %s ==="),
            *System->GetPathName());
    }
    else
    {
        OnProgress.Broadcast(TEXT("Build"), 1.0f, TEXT("Failed to build Niagara System"));
    }

    OnComplete.Broadcast(System != nullptr, System);

    if (ActiveBatch.IsValid())
    {
        if (System) ActiveBatch->Results.Add(System);
        ProcessNextBatchItem();
    }
}

void UHktVFXGeneratorSubsystem::GenerateBatch(const TArray<FHktVFXGenerationRequest>& Requests)
{
    if (Requests.Num() == 0) return;

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Starting batch generation: %d items"), Requests.Num());

    ActiveBatch = MakeShared<FHktVFXBatchState>();
    ActiveBatch->Requests = Requests;
    ActiveBatch->CurrentIndex = 0;

    ProcessNextBatchItem();
}

void UHktVFXGeneratorSubsystem::ProcessNextBatchItem()
{
    if (!ActiveBatch.IsValid()) return;

    if (ActiveBatch->CurrentIndex >= ActiveBatch->Requests.Num())
    {
        UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Batch complete: %d/%d succeeded"),
            ActiveBatch->Results.Num(), ActiveBatch->Requests.Num());
        ActiveBatch.Reset();
        return;
    }

    const FHktVFXGenerationRequest& Request = ActiveBatch->Requests[ActiveBatch->CurrentIndex];
    ActiveBatch->CurrentIndex++;

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Batch item %d/%d: %s"),
        ActiveBatch->CurrentIndex, ActiveBatch->Requests.Num(),
        *Request.Intent.GetAssetKey());

    ProcessSingleRequest(Request);
}

void UHktVFXGeneratorSubsystem::QuickGenerate(const FString& Description)
{
    FHktVFXIntent Intent = ParseQuickDescription(Description);

    FHktVFXGenerationRequest Request;
    Request.Intent = Intent;
    Request.OutputDirectory = Settings->DefaultOutputDirectory;
    Request.TextureResolution = Settings->DefaultTextureResolution;

    GenerateVFX(Request);
}

FHktVFXIntent UHktVFXGeneratorSubsystem::ParseQuickDescription(const FString& Description) const
{
    FHktVFXIntent Intent;
    FString Desc = Description.ToLower();

    if (Desc.Contains(TEXT("explo")))        Intent.EventType = EHktVFXEventType::Explosion;
    else if (Desc.Contains(TEXT("hit")))     Intent.EventType = EHktVFXEventType::ProjectileHit;
    else if (Desc.Contains(TEXT("trail")))   Intent.EventType = EHktVFXEventType::ProjectileTrail;
    else if (Desc.Contains(TEXT("area")))    Intent.EventType = EHktVFXEventType::AreaEffect;
    else if (Desc.Contains(TEXT("buff")))    Intent.EventType = EHktVFXEventType::Buff;
    else if (Desc.Contains(TEXT("heal")))    Intent.EventType = EHktVFXEventType::Heal;
    else if (Desc.Contains(TEXT("shield")))  Intent.EventType = EHktVFXEventType::Shield;
    else if (Desc.Contains(TEXT("summon")))  Intent.EventType = EHktVFXEventType::Summon;
    else if (Desc.Contains(TEXT("death")))   Intent.EventType = EHktVFXEventType::Death;
    else                                     Intent.EventType = EHktVFXEventType::Custom;

    if (Desc.Contains(TEXT("fire")) || Desc.Contains(TEXT("flame")))
        Intent.Element = EHktVFXElement::Fire;
    else if (Desc.Contains(TEXT("ice")) || Desc.Contains(TEXT("frost")))
        Intent.Element = EHktVFXElement::Ice;
    else if (Desc.Contains(TEXT("lightning")) || Desc.Contains(TEXT("electric")))
        Intent.Element = EHktVFXElement::Lightning;
    else if (Desc.Contains(TEXT("dark")) || Desc.Contains(TEXT("shadow")))
        Intent.Element = EHktVFXElement::Dark;
    else if (Desc.Contains(TEXT("holy")) || Desc.Contains(TEXT("light")) || Desc.Contains(TEXT("divine")))
        Intent.Element = EHktVFXElement::Holy;
    else if (Desc.Contains(TEXT("poison")) || Desc.Contains(TEXT("toxic")))
        Intent.Element = EHktVFXElement::Poison;
    else if (Desc.Contains(TEXT("water")))
        Intent.Element = EHktVFXElement::Water;
    else if (Desc.Contains(TEXT("earth")) || Desc.Contains(TEXT("rock")))
        Intent.Element = EHktVFXElement::Earth;
    else if (Desc.Contains(TEXT("wind")))
        Intent.Element = EHktVFXElement::Wind;
    else if (Desc.Contains(TEXT("arcane")) || Desc.Contains(TEXT("magic")))
        Intent.Element = EHktVFXElement::Arcane;
    else if (Desc.Contains(TEXT("nature")) || Desc.Contains(TEXT("leaf")))
        Intent.Element = EHktVFXElement::Nature;

    if (Desc.Contains(TEXT("weak")) || Desc.Contains(TEXT("small")) || Desc.Contains(TEXT("subtle")))
        Intent.Intensity = 0.3f;
    else if (Desc.Contains(TEXT("strong")) || Desc.Contains(TEXT("powerful")) || Desc.Contains(TEXT("big")))
        Intent.Intensity = 0.8f;
    else if (Desc.Contains(TEXT("massive")) || Desc.Contains(TEXT("epic")) || Desc.Contains(TEXT("huge")))
        Intent.Intensity = 1.0f;

    if (Desc.Contains(TEXT("stone")))      Intent.SurfaceType = EHktVFXSurfaceType::Stone;
    else if (Desc.Contains(TEXT("metal"))) Intent.SurfaceType = EHktVFXSurfaceType::Metal;
    else if (Desc.Contains(TEXT("water"))) Intent.SurfaceType = EHktVFXSurfaceType::Water;
    else if (Desc.Contains(TEXT("snow")))  Intent.SurfaceType = EHktVFXSurfaceType::Snow;

    Intent.CustomDescription = Description;

    return Intent;
}

void UHktVFXGeneratorSubsystem::GeneratePresetBank(
    const TArray<EHktVFXEventType>& EventTypes,
    const TArray<EHktVFXElement>& Elements,
    const TArray<float>& Intensities)
{
    TArray<FHktVFXGenerationRequest> Requests;

    for (EHktVFXEventType Event : EventTypes)
    {
        for (EHktVFXElement Element : Elements)
        {
            for (float Intensity : Intensities)
            {
                FHktVFXGenerationRequest Req;
                Req.Intent.EventType = Event;
                Req.Intent.Element = Element;
                Req.Intent.Intensity = Intensity;
                Req.OutputDirectory = Settings->DefaultOutputDirectory /
                    UEnum::GetValueAsString(Event).RightChop(20);  // "EHktVFXEventType::" 제거
                Req.TextureResolution = Settings->DefaultTextureResolution;
                Requests.Add(Req);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[VFXGenerator] Preset bank: %d combinations to generate"), Requests.Num());
    GenerateBatch(Requests);
}
