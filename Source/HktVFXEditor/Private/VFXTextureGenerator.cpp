// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "VFXTextureGenerator.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/Base64.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/TextureFactory.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

// ============================================================================
// VFX 텍스처 전용 프롬프트 빌드
// ============================================================================
FString FVFXTextureGenerator::BuildFinalPrompt(const FVFXTextureRequest& Request) const
{
    FString Prompt = Request.Prompt;

    // VFX 텍스처 공통 품질 태그 추가
    Prompt += TEXT(", game VFX texture, black background, high contrast, centered");
    Prompt += TEXT(", professional quality, clean edges");

    // 텍스처 타입별 추가 태그
    if (Request.TextureType == TEXT("flipbook"))
    {
        int32 Grid = FMath::Max(Request.FlipbookGridSize, 2);
        Prompt += FString::Printf(TEXT(", sprite sheet %dx%d grid, sequential animation frames"), Grid, Grid);
    }
    else if (Request.TextureType == TEXT("normal"))
    {
        Prompt = FString::Printf(TEXT("normal map of %s, blue-purple tones, 3d surface detail"), *Request.Prompt);
    }
    else if (Request.TextureType == TEXT("mask"))
    {
        Prompt = FString::Printf(TEXT("grayscale mask texture of %s, white on black background"), *Request.Prompt);
    }

    return Prompt;
}

FString FVFXTextureGenerator::BuildFinalNegativePrompt(const FVFXTextureRequest& Request) const
{
    FString Negative = Request.NegativePrompt;
    if (!Negative.IsEmpty()) Negative += TEXT(", ");
    Negative += TEXT("text, watermark, signature, blurry, low quality, photo, realistic face, human");
    return Negative;
}

// ============================================================================
// AUTOMATIC1111 Stable Diffusion WebUI API 호출
// ============================================================================
void FVFXTextureGenerator::CallSD_WebUI(
    const FString& Prompt, const FString& NegativePrompt,
    int32 Width, int32 Height,
    TFunction<void(bool, const TArray<uint8>&)> OnComplete)
{
    auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(FString::Printf(TEXT("%s/sdapi/v1/txt2img"), *Settings.ServerURL));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TSharedPtr<FJsonObject> Body = MakeShareable(new FJsonObject);
    Body->SetStringField(TEXT("prompt"), Prompt);
    Body->SetStringField(TEXT("negative_prompt"), NegativePrompt);
    Body->SetNumberField(TEXT("width"), Width);
    Body->SetNumberField(TEXT("height"), Height);
    Body->SetNumberField(TEXT("steps"), Settings.Steps);
    Body->SetNumberField(TEXT("cfg_scale"), Settings.CFGScale);
    Body->SetNumberField(TEXT("batch_size"), 1);
    Body->SetStringField(TEXT("sampler_name"), TEXT("DPM++ 2M Karras"));

    FString BodyString;
    auto Writer = TJsonWriterFactory<>::Create(&BodyString);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Request->SetContentAsString(BodyString);

    Request->OnProcessRequestComplete().BindLambda(
        [OnComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
        {
            TArray<uint8> EmptyData;
            if (!bConnected || !Resp.IsValid() || Resp->GetResponseCode() != 200)
            {
                UE_LOG(LogTemp, Error, TEXT("[VFXTexGen] SD WebUI API call failed: %s"),
                    Resp.IsValid() ? *Resp->GetContentAsString() : TEXT("No connection"));
                OnComplete(false, EmptyData);
                return;
            }

            TSharedPtr<FJsonObject> ResponseObj;
            auto Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, ResponseObj))
            {
                OnComplete(false, EmptyData);
                return;
            }

            auto Images = ResponseObj->GetArrayField(TEXT("images"));
            if (Images.Num() == 0)
            {
                OnComplete(false, EmptyData);
                return;
            }

            // Base64 디코딩
            FString Base64Image = Images[0]->AsString();
            TArray<uint8> ImageData;
            FBase64::Decode(Base64Image, ImageData);

            UE_LOG(LogTemp, Log, TEXT("[VFXTexGen] Generated texture: %d bytes"), ImageData.Num());
            OnComplete(true, ImageData);
        });

    Request->ProcessRequest();
}

// ============================================================================
// ComfyUI API 호출 (기본 txt2img 워크플로우)
// ============================================================================
void FVFXTextureGenerator::CallComfyUI(
    const FString& Prompt, const FString& NegativePrompt,
    int32 Width, int32 Height,
    TFunction<void(bool, const TArray<uint8>&)> OnComplete)
{
    // ComfyUI는 워크플로우 기반이므로 JSON 워크플로우를 구성
    // 여기서는 기본 txt2img 워크플로우를 하드코딩

    FString WorkflowJSON = FString::Printf(TEXT(R"({
        "prompt": {
            "3": {
                "class_type": "KSampler",
                "inputs": {
                    "seed": %d,
                    "steps": %d,
                    "cfg": %.1f,
                    "sampler_name": "dpmpp_2m",
                    "scheduler": "karras",
                    "denoise": 1.0,
                    "model": ["4", 0],
                    "positive": ["6", 0],
                    "negative": ["7", 0],
                    "latent_image": ["5", 0]
                }
            },
            "4": {
                "class_type": "CheckpointLoaderSimple",
                "inputs": { "ckpt_name": "%s" }
            },
            "5": {
                "class_type": "EmptyLatentImage",
                "inputs": { "width": %d, "height": %d, "batch_size": 1 }
            },
            "6": {
                "class_type": "CLIPTextEncode",
                "inputs": { "text": "%s", "clip": ["4", 1] }
            },
            "7": {
                "class_type": "CLIPTextEncode",
                "inputs": { "text": "%s", "clip": ["4", 1] }
            },
            "8": {
                "class_type": "VAEDecode",
                "inputs": { "samples": ["3", 0], "vae": ["4", 2] }
            },
            "9": {
                "class_type": "SaveImage",
                "inputs": { "filename_prefix": "vfx_gen", "images": ["8", 0] }
            }
        }
    })"),
        FMath::RandRange(0, 999999999),
        Settings.Steps,
        Settings.CFGScale,
        *Settings.Model,
        Width, Height,
        *Prompt.ReplaceCharWithEscapedChar(),
        *NegativePrompt.ReplaceCharWithEscapedChar()
    );

    auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(FString::Printf(TEXT("%s/api/prompt"), *Settings.ServerURL));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(WorkflowJSON);

    // ComfyUI는 비동기적이므로 prompt_id를 받고 폴링해야 함
    // 간소화를 위해 동기적 패턴으로 구현 (실제 프로덕션에서는 폴링 필요)
    Request->OnProcessRequestComplete().BindLambda(
        [this, OnComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
        {
            TArray<uint8> EmptyData;
            if (!bConnected || !Resp.IsValid() || Resp->GetResponseCode() != 200)
            {
                OnComplete(false, EmptyData);
                return;
            }

            TSharedPtr<FJsonObject> ResponseObj;
            auto Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
            FJsonSerializer::Deserialize(Reader, ResponseObj);

            FString PromptId = ResponseObj->GetStringField(TEXT("prompt_id"));

            // 결과 폴링 (간소화 - 실제로는 WebSocket 또는 타이머 사용)
            PollComfyUIResult(PromptId, OnComplete);
        });

    Request->ProcessRequest();
}

// ComfyUI 결과 폴링은 FTimerManager나 FTSTicker를 사용해 구현
// 여기서는 헤더만 선언 (구현 생략 - SD WebUI가 더 간단하므로 추천)

// ============================================================================
// PNG 데이터 → UTexture2D 에셋 임포트
// ============================================================================
UTexture2D* FVFXTextureGenerator::ImportTextureFromPNG(
    const TArray<uint8>& PNGData,
    const FString& AssetPath,
    const FString& TextureName)
{
    // 1. 임시 파일로 저장
    FString TempDir = FPaths::ProjectSavedDir() / TEXT("Temp/AIVFXTextures");
    IFileManager::Get().MakeDirectory(*TempDir, true);
    FString TempFilePath = TempDir / (TextureName + TEXT(".png"));
    FFileHelper::SaveArrayToFile(PNGData, *TempFilePath);

    // 2. TextureFactory로 임포트
    UTextureFactory* Factory = NewObject<UTextureFactory>();
    Factory->AddToRoot(); // GC 방지
    Factory->SuppressImportOverwriteDialog();

    // 임포트 설정
    Factory->NoCompression = false;
    Factory->bDeferCompression = true;

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    TArray<FString> FilePaths;
    FilePaths.Add(TempFilePath);

    TArray<UObject*> ImportedAssets = AssetTools.ImportAssets(FilePaths, AssetPath);

    Factory->RemoveFromRoot();

    // 3. 임시 파일 정리
    IFileManager::Get().Delete(*TempFilePath);

    if (ImportedAssets.Num() > 0)
    {
        UTexture2D* Texture = Cast<UTexture2D>(ImportedAssets[0]);
        if (Texture)
        {
            // 에셋 이름 변경
            FString DesiredPath = AssetPath / TextureName;
            // Rename은 이미 ImportAssets에서 처리됨

            UE_LOG(LogTemp, Log, TEXT("[VFXTexGen] Imported texture: %s"), *Texture->GetPathName());
            return Texture;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("[VFXTexGen] Failed to import texture: %s"), *TextureName);
    return nullptr;
}

// ============================================================================
// VFX 용도에 맞게 텍스처 설정
// ============================================================================
void FVFXTextureGenerator::ConfigureTextureForVFX(UTexture2D* Texture, const FString& BlendMode)
{
    if (!Texture) return;

    // VFX 텍스처 공통 설정
    Texture->MipGenSettings = TMGS_NoMipmaps;   // 파티클 텍스처는 밉맵 불필요한 경우 많음
    Texture->LODGroup = TEXTUREGROUP_Effects;
    Texture->SRGB = false;                       // 리니어 컬러 스페이스
    Texture->CompressionSettings = TC_HDR;       // HDR 지원

    if (BlendMode == TEXT("additive"))
    {
        // Additive 블렌딩용: 검은 배경이 투명하게 됨
        Texture->CompressionSettings = TC_Default;
        Texture->SRGB = true;
    }
    else if (BlendMode == TEXT("translucent"))
    {
        // 알파 채널 사용
        Texture->CompressionSettings = TC_EditorIcon; // 알파 보존
    }

    Texture->UpdateResource();
    Texture->PostEditChange();
    Texture->MarkPackageDirty();
}

// ============================================================================
// 단일 텍스처 생성
// ============================================================================
void FVFXTextureGenerator::GenerateTexture(
    const FVFXTextureRequest& Request,
    const FString& OutputPath,
    int32 Resolution,
    FOnTextureGenerated OnComplete)
{
    FString FinalPrompt = BuildFinalPrompt(Request);
    FString FinalNegative = BuildFinalNegativePrompt(Request);

    UE_LOG(LogTemp, Log, TEXT("[VFXTexGen] Generating: %s"), *FinalPrompt);

    int32 Width = Resolution;
    int32 Height = Resolution;

    // Flipbook은 전체 시트 크기
    if (Request.TextureType == TEXT("flipbook") && Request.FlipbookGridSize > 1)
    {
        Width = Resolution * Request.FlipbookGridSize;
        Height = Resolution * Request.FlipbookGridSize;
    }

    auto OnImageGenerated = [this, Request, OutputPath, OnComplete](bool bSuccess, const TArray<uint8>& ImageData)
    {
        if (!bSuccess)
        {
            OnComplete.ExecuteIfBound(false, nullptr);
            return;
        }

        FString TextureName = FString::Printf(TEXT("T_%s_%s"),
            *Request.EmitterName, *Request.TextureType);

        UTexture2D* Texture = ImportTextureFromPNG(ImageData, OutputPath, TextureName);
        if (Texture)
        {
            ConfigureTextureForVFX(Texture, TEXT("additive")); // 기본 additive
        }

        OnComplete.ExecuteIfBound(Texture != nullptr, Texture);
    };

    switch (Settings.Provider)
    {
    case EImageGenProvider::StableDiffusionWebUI:
        CallSD_WebUI(FinalPrompt, FinalNegative, Width, Height, OnImageGenerated);
        break;
    case EImageGenProvider::ComfyUI:
        CallComfyUI(FinalPrompt, FinalNegative, Width, Height, OnImageGenerated);
        break;
    }
}

// ============================================================================
// 배치 텍스처 생성 (순차 처리)
// ============================================================================
void FVFXTextureGenerator::GenerateAllTextures(
    const TArray<FVFXTextureRequest>& Requests,
    const FString& OutputDirectory,
    int32 Resolution,
    FOnAllTexturesGenerated OnComplete)
{
    if (Requests.Num() == 0)
    {
        TMap<FString, UTexture2D*> Empty;
        OnComplete.ExecuteIfBound(true, Empty);
        return;
    }

    auto Batch = MakeShared<FPendingBatch>();
    Batch->Requests = Requests;
    Batch->OutputDirectory = OutputDirectory;
    Batch->Resolution = Resolution;
    Batch->OnComplete = OnComplete;

    ProcessNextInBatch(Batch);
}

void FVFXTextureGenerator::ProcessNextInBatch(TSharedPtr<FPendingBatch> Batch)
{
    if (Batch->CurrentIndex >= Batch->Requests.Num())
    {
        // 모든 텍스처 생성 완료
        UE_LOG(LogTemp, Log, TEXT("[VFXTexGen] All %d textures generated"), Batch->Results.Num());
        Batch->OnComplete.ExecuteIfBound(true, Batch->Results);
        return;
    }

    const FVFXTextureRequest& CurrentReq = Batch->Requests[Batch->CurrentIndex];
    FString TexOutputDir = Batch->OutputDirectory / TEXT("Textures");

    FOnTextureGenerated OnSingleDone;
    OnSingleDone.BindLambda([this, Batch, CurrentReq](bool bSuccess, UTexture2D* Texture)
    {
        if (bSuccess && Texture)
        {
            Batch->Results.Add(CurrentReq.EmitterName, Texture);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[VFXTexGen] Failed to generate texture for emitter: %s"),
                *CurrentReq.EmitterName);
        }

        Batch->CurrentIndex++;
        ProcessNextInBatch(Batch);
    });

    GenerateTexture(CurrentReq, TexOutputDir, Batch->Resolution, OnSingleDone);
}