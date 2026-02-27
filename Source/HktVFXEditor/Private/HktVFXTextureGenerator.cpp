// Copyright Hkt Studios, Inc. All Rights Reserved.

#include "HktVFXTextureGenerator.h"
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

void FHktVFXTextureGenerator::SetSettings(const FHktImageGenSettings& InSettings)
{
    Settings = InSettings;
}

// ============================================================================
// VFX 텍스처 전용 프롬프트 빌드
// ============================================================================
FString FHktVFXTextureGenerator::BuildFinalPrompt(const FHktVFXTextureRequest& Request) const
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

FString FHktVFXTextureGenerator::BuildFinalNegativePrompt(const FHktVFXTextureRequest& Request) const
{
    FString Negative = Request.NegativePrompt;
    if (!Negative.IsEmpty()) Negative += TEXT(", ");
    Negative += TEXT("text, watermark, signature, blurry, low quality, photo, realistic face, human");
    return Negative;
}

// ============================================================================
// AUTOMATIC1111 Stable Diffusion WebUI API 호출
// ============================================================================
void FHktVFXTextureGenerator::CallSD_WebUI(
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

#include "HAL/PlatformFileManager.h"

// ============================================================================
// ComfyUI — 미지원 (에러 로그 + early return, 링커 에러 방지용 stub)
// ============================================================================
void FHktVFXTextureGenerator::CallComfyUI(
    const FString& Prompt, const FString& NegativePrompt,
    int32 Width, int32 Height,
    TFunction<void(bool, const TArray<uint8>&)> OnComplete)
{
    UE_LOG(LogTemp, Error, TEXT("[VFXTexGen] ComfyUI is not supported yet. "
        "Please use Stable Diffusion WebUI (AUTOMATIC1111) instead. "
        "Change the provider in Project Settings > Plugins > Hkt VFX > Image Generation."));

    TArray<uint8> EmptyData;
    OnComplete(false, EmptyData);
}

void FHktVFXTextureGenerator::PollComfyUIResult(
    const FString& PromptId,
    TFunction<void(bool, const TArray<uint8>&)> OnComplete)
{
    // ComfyUI 미지원 — 호출되지 않아야 하지만 링커 에러 방지용 stub
    UE_LOG(LogTemp, Error, TEXT("[VFXTexGen] PollComfyUIResult called but ComfyUI is not supported."));

    TArray<uint8> EmptyData;
    OnComplete(false, EmptyData);
}

// ============================================================================
// PNG 데이터 → UTexture2D 에셋 임포트
// ============================================================================
UTexture2D* FHktVFXTextureGenerator::ImportTextureFromPNG(
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
void FHktVFXTextureGenerator::ConfigureTextureForVFX(UTexture2D* Texture, const FString& BlendMode)
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
void FHktVFXTextureGenerator::GenerateTexture(
    const FHktVFXTextureRequest& Request,
    const FString& OutputPath,
    int32 Resolution,
    FOnHktTextureGenerated OnComplete)
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
    case EHktImageGenProvider::StableDiffusionWebUI:
        CallSD_WebUI(FinalPrompt, FinalNegative, Width, Height, OnImageGenerated);
        break;
    case EHktImageGenProvider::ComfyUI:
        CallComfyUI(FinalPrompt, FinalNegative, Width, Height, OnImageGenerated);
        break;
    }
}

// ============================================================================
// 배치 텍스처 생성 (순차 처리)
// ============================================================================
void FHktVFXTextureGenerator::GenerateAllTextures(
    const TArray<FHktVFXTextureRequest>& Requests,
    const FString& OutputDirectory,
    int32 Resolution,
    FOnHktAllTexturesGenerated OnComplete)
{
    if (Requests.Num() == 0)
    {
        TMap<FString, UTexture2D*> Empty;
        OnComplete.ExecuteIfBound(true, Empty);
        return;
    }

    auto Batch = MakeShared<FHktVFXPendingBatch>();
    Batch->Requests = Requests;
    Batch->OutputDirectory = OutputDirectory;
    Batch->Resolution = Resolution;
    Batch->OnComplete = OnComplete;

    ProcessNextInBatch(Batch);
}

void FHktVFXTextureGenerator::ProcessNextInBatch(TSharedPtr<FHktVFXPendingBatch> Batch)
{
    if (Batch->CurrentIndex >= Batch->Requests.Num())
    {
        // 모든 텍스처 생성 완료
        UE_LOG(LogTemp, Log, TEXT("[VFXTexGen] All %d textures generated"), Batch->Results.Num());
        Batch->OnComplete.ExecuteIfBound(true, Batch->Results);
        return;
    }

    const FHktVFXTextureRequest& CurrentReq = Batch->Requests[Batch->CurrentIndex];
    FString TexOutputDir = Batch->OutputDirectory / TEXT("Textures");

    FOnHktTextureGenerated OnSingleDone;
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