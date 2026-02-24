// Copyright Hkt Studios, Inc. All Rights Reserved.
// Config → 실제 UNiagaraSystem 에셋 빌드

#include "VFXNiagaraBuilder.h"

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraEditorModule.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraParameterStore.h"
#include "NiagaraComponent.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"

#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Factories/MaterialFactoryNew.h"
#include "PackageTools.h"

// ============================================================================
// 메인 빌드 엔트리
// ============================================================================
UNiagaraSystem* FVFXNiagaraBuilder::BuildNiagaraSystem(
    const FVFXNiagaraConfig& Config,
    const TMap<FString, UTexture2D*>& Textures,
    const FString& OutputDirectory)
{
    if (!Config.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[NiagaraBuilder] Invalid config"));
        return nullptr;
    }

    // 1. 패키지 & 시스템 생성
    FString SystemName = Config.SystemName.IsEmpty()
        ? TEXT("NS_Generated") : FString::Printf(TEXT("NS_%s"), *Config.SystemName);
    FString PackagePath = OutputDirectory / SystemName;

    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("[NiagaraBuilder] Failed to create package: %s"), *PackagePath);
        return nullptr;
    }

    UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, *SystemName,
        RF_Public | RF_Standalone);

    if (!System)
    {
        UE_LOG(LogTemp, Error, TEXT("[NiagaraBuilder] Failed to create NiagaraSystem"));
        return nullptr;
    }

    // 시스템 초기 설정
    System->SetAutoDestroy(true);

    // 2. 각 에미터 구성
    for (int32 i = 0; i < Config.Emitters.Num(); ++i)
    {
        const FVFXEmitterConfig& EmitterConfig = Config.Emitters[i];
        UTexture2D* Texture = Textures.FindRef(EmitterConfig.Name);

        UE_LOG(LogTemp, Log, TEXT("[NiagaraBuilder] Building emitter %d: %s (purpose: %s)"),
            i, *EmitterConfig.Name, *EmitterConfig.Purpose);

        ConfigureEmitter(System, i, EmitterConfig, Texture);
    }

    // 3. Exposed Parameters
    SetupExposedParameters(System, Config.ExposedParameters);

    // 4. 컴파일 & 저장
    System->RequestCompile(false);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(
        PackagePath, FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, System, *PackageFileName, SaveArgs);

    // 에셋 레지스트리에 등록
    FAssetRegistryModule::AssetCreated(System);

    UE_LOG(LogTemp, Log, TEXT("[NiagaraBuilder] ✓ Built NiagaraSystem: %s (%d emitters)"),
        *PackagePath, Config.Emitters.Num());

    return System;
}

// ============================================================================
// 에미터 전체 구성
// ============================================================================
void FVFXNiagaraBuilder::ConfigureEmitter(
    UNiagaraSystem* System,
    int32 EmitterIndex,
    const FVFXEmitterConfig& Config,
    UTexture2D* Texture)
{
    /*
     * UE5 Niagara 에디터 API를 통한 에미터 구성
     *
     * 핵심 패턴:
     * 1. 템플릿 에미터를 기반으로 새 에미터 추가
     * 2. 모듈별 파라미터 오버라이드
     * 3. 렌더러 프로퍼티 설정
     *
     * NOTE: UE5 버전에 따라 Niagara 에디터 API가 다를 수 있음.
     * 아래는 UE 5.3+ 기준이며, 5.6에서 동작 확인 필요.
     * FNiagaraEditorModule의 CreateEmitter 또는 직접 UNiagaraEmitter 생성을 사용.
     */

    // --- 방법 A: 기본 템플릿 기반 (권장) ---
    // Niagara의 내장 Simple Sprite Burst / Simple Sprite Rate 템플릿 활용

    FString TemplatePath;
    if (Config.Render.RendererType == TEXT("ribbon"))
    {
        TemplatePath = TEXT("/Niagara/Templates/SimpleRibbon");
    }
    else if (Config.Render.RendererType == TEXT("light"))
    {
        TemplatePath = TEXT("/Niagara/Templates/SimpleLight");
    }
    else
    {
        // Sprite 기본
        if (Config.Spawn.Mode == TEXT("burst"))
            TemplatePath = TEXT("/Niagara/Templates/SimpleSpriteBurst");
        else
            TemplatePath = TEXT("/Niagara/Templates/SimpleSpriteRate");
    }

    // 템플릿 로드
    UNiagaraEmitter* TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
    if (!TemplateEmitter)
    {
        // 폴백: 기본 빈 에미터
        UE_LOG(LogTemp, Warning, TEXT("[NiagaraBuilder] Template not found: %s, using default"),
            *TemplatePath);
        TemplatePath = TEXT("/Niagara/Templates/SimpleSpriteBurst");
        TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
    }

    if (!TemplateEmitter)
    {
        UE_LOG(LogTemp, Error, TEXT("[NiagaraBuilder] Cannot load any emitter template"));
        return;
    }

    // 시스템에 에미터 추가
    FNiagaraEmitterHandle EmitterHandle = System->AddEmitterHandle(*TemplateEmitter, FName(*Config.Name));

    // 각 모듈 설정
    SetupSpawnModule(System, EmitterIndex, Config.Spawn);
    SetupInitializeModule(System, EmitterIndex, Config.Init);
    SetupUpdateModules(System, EmitterIndex, Config.Update);
    SetupRenderer(System, EmitterIndex, Config.Render, Texture);
}

// ============================================================================
// 스폰 설정
// ============================================================================
void FVFXNiagaraBuilder::SetupSpawnModule(UNiagaraSystem* System, int32 EmitterIndex,
    const FVFXEmitterSpawnConfig& Config)
{
    if (Config.Mode == TEXT("rate"))
    {
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("SpawnRate"), TEXT("SpawnRate"), Config.Rate);
    }
    else // burst
    {
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("SpawnBurstInstantaneous"), TEXT("SpawnCount"),
            static_cast<float>(Config.BurstCount));

        if (Config.BurstDelay > 0.f)
        {
            SetNiagaraVariableFloat(System, EmitterIndex,
                TEXT("SpawnBurstInstantaneous"), TEXT("SpawnTime"), Config.BurstDelay);
        }
    }
}

// ============================================================================
// 초기화 설정
// ============================================================================
void FVFXNiagaraBuilder::SetupInitializeModule(UNiagaraSystem* System, int32 EmitterIndex,
    const FVFXEmitterInitConfig& Config)
{
    const FString Module = TEXT("InitializeParticle");

    // Lifetime
    SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("Lifetime.Minimum"), Config.LifetimeMin);
    SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("Lifetime.Maximum"), Config.LifetimeMax);

    // Size (Uniform)
    SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("SpriteSize.Minimum.X"), Config.SizeMin);
    SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("SpriteSize.Minimum.Y"), Config.SizeMin);
    SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("SpriteSize.Maximum.X"), Config.SizeMax);
    SetNiagaraVariableFloat(System, EmitterIndex, Module, TEXT("SpriteSize.Maximum.Y"), Config.SizeMax);

    // Velocity
    SetNiagaraVariableVec3(System, EmitterIndex, Module, TEXT("Velocity.Minimum"), Config.VelocityMin);
    SetNiagaraVariableVec3(System, EmitterIndex, Module, TEXT("Velocity.Maximum"), Config.VelocityMax);

    // Color
    SetNiagaraVariableColor(System, EmitterIndex, Module, TEXT("Color"), Config.Color);
}

// ============================================================================
// 업데이트 모듈 설정
// ============================================================================
void FVFXNiagaraBuilder::SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex,
    const FVFXEmitterUpdateConfig& Config)
{
    // Gravity
    if (!Config.Gravity.IsNearlyZero())
    {
        SetNiagaraVariableVec3(System, EmitterIndex,
            TEXT("Gravity Force"), TEXT("Gravity"), Config.Gravity);
    }

    // Drag
    if (Config.Drag > 0.f)
    {
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("Drag"), TEXT("Drag"), Config.Drag);
    }

    // Rotation
    if (Config.RotationRateMax > 0.f)
    {
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("SpriteRotationRate"), TEXT("RotationRate.Minimum"), Config.RotationRateMin);
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("SpriteRotationRate"), TEXT("RotationRate.Maximum"), Config.RotationRateMax);
    }

    // Curl Noise / Turbulence
    if (Config.NoiseStrength > 0.f)
    {
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("Curl Noise Force"), TEXT("NoisStrength"), Config.NoiseStrength);
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("Curl Noise Force"), TEXT("NoiseFrequency"), Config.NoiseFrequency);
    }

    // Orbit (Point Attractor + Vortex 조합으로 구현)
    if (Config.bOrbitCenter && Config.OrbitSpeed > 0.f)
    {
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("Vortex Force"), TEXT("VortexStrength"), Config.OrbitSpeed);
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("Vortex Force"), TEXT("VortexRadius"), Config.OrbitRadius);
    }

    // Attract to center
    if (Config.bAttractToCenter && Config.AttractionStrength > 0.f)
    {
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("Point Attraction Force"), TEXT("AttractionStrength"), Config.AttractionStrength);
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("Point Attraction Force"), TEXT("AttractionRadius"), Config.OrbitRadius);
    }

    /*
     * Size/Color/Opacity 커브:
     *
     * Niagara에서 시간 기반 커브를 적용하는 두 가지 방법:
     * 1. Scale Color/Size By Curve 모듈 사용 (간단)
     * 2. Dynamic Input으로 Curve 에셋 참조 (유연)
     *
     * 여기서는 모듈 기반 접근 사용.
     * Niagara의 "Scale Sprite Size" / "Scale Color" 모듈이
     * 내부적으로 CurveFloat를 받을 수 있음.
     */

    // Size curve → Scale Sprite Size 모듈
    if (Config.SizeCurve.Num() >= 2)
    {
        // Niagara의 "Scale Sprite Size by Speed" 또는 커스텀 모듈을 통해
        // NormalizedAge 기반 커브를 설정.
        // 간단한 방법: 시작/끝 스케일만 설정
        float StartScale = Config.SizeCurve[0].Value;
        float EndScale = Config.SizeCurve.Last().Value;

        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("ScaleSpriteSize"), TEXT("ScaleFactor.Minimum"), StartScale);
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("ScaleSpriteSize"), TEXT("ScaleFactor.Maximum"), EndScale);
    }

    // Opacity curve → Scale Color 모듈의 Alpha
    if (Config.OpacityCurve.Num() >= 2)
    {
        // Niagara에서 알파 페이드는 보통 "Scale Color" 모듈 내 커브로 처리
        // 또는 "Alpha" 채널에 NormalizedAge 기반 커브 매핑
        float StartAlpha = Config.OpacityCurve[0].Value;
        float EndAlpha = Config.OpacityCurve.Last().Value;

        // 간단한 linear fade: 시작→끝 보간
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("ScaleColor"), TEXT("AlphaScale.Start"), StartAlpha);
        SetNiagaraVariableFloat(System, EmitterIndex,
            TEXT("ScaleColor"), TEXT("AlphaScale.End"), EndAlpha);
    }
}

// ============================================================================
// 렌더러 설정
// ============================================================================
void FVFXNiagaraBuilder::SetupRenderer(UNiagaraSystem* System, int32 EmitterIndex,
    const FVFXEmitterRenderConfig& Config, UTexture2D* Texture)
{
    /*
     * 렌더러 타입에 따라 적절한 Properties 클래스를 설정
     *
     * 템플릿 기반으로 에미터를 만들었으므로, 기본 렌더러가 이미 있음.
     * 여기서는 해당 렌더러의 프로퍼티를 수정.
     */

    const auto& EmitterHandles = System->GetEmitterHandles();
    if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

    UNiagaraEmitter* Emitter = EmitterHandles[EmitterIndex].GetInstance().Emitter;
    if (!Emitter) return;

    // 렌더러 프로퍼티 가져오기
    auto& RendererProperties = Emitter->GetRenderers();
    if (RendererProperties.Num() == 0) return;

    UNiagaraRendererProperties* Renderer = RendererProperties[0];

    // --- Sprite Renderer ---
    if (Config.RendererType == TEXT("sprite"))
    {
        if (UNiagaraSpriteRendererProperties* SpriteRenderer =
            Cast<UNiagaraSpriteRendererProperties>(Renderer))
        {
            SpriteRenderer->SortOrderHint = Config.SortOrder;

            // Alignment
            if (Config.Alignment == TEXT("velocity_aligned"))
            {
                SpriteRenderer->Alignment = ENiagaraSpriteAlignment::VelocityAligned;
            }
            else
            {
                SpriteRenderer->Alignment = ENiagaraSpriteAlignment::Unaligned;
            }

            // SubUV
            if (Config.SubUVFrames > 1)
            {
                int32 GridDim = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Config.SubUVFrames)));
                SpriteRenderer->SubImageSize = FVector2D(GridDim, GridDim);
            }

            // 머티리얼 (텍스처 포함)
            if (Texture)
            {
                UMaterialInterface* Mat = GetOrCreateVFXMaterial(
                    Config.BlendMode, Config.TextureStyle, Texture,
                    System->GetPathName());
                if (Mat)
                {
                    SpriteRenderer->Material = Mat;
                }
            }
        }
    }
    // --- Ribbon Renderer ---
    else if (Config.RendererType == TEXT("ribbon"))
    {
        if (UNiagaraRibbonRendererProperties* RibbonRenderer =
            Cast<UNiagaraRibbonRendererProperties>(Renderer))
        {
            // 리본 설정은 기본값 유지하고 머티리얼만 설정
            if (Texture)
            {
                UMaterialInterface* Mat = GetOrCreateVFXMaterial(
                    Config.BlendMode, Config.TextureStyle, Texture,
                    System->GetPathName());
                if (Mat) RibbonRenderer->Material = Mat;
            }
        }
    }
    // --- Light Renderer ---
    else if (Config.RendererType == TEXT("light"))
    {
        if (UNiagaraLightRendererProperties* LightRenderer =
            Cast<UNiagaraLightRendererProperties>(Renderer))
        {
            LightRenderer->RadiusScale = Config.LightRadius / 100.f; // 정규화
            LightRenderer->ColorAdd = FVector3f(
                Config.LightIntensity, Config.LightIntensity, Config.LightIntensity);
        }
    }
} 

// ============================================================================
// VFX 머티리얼 생성
// ============================================================================
UMaterialInterface* FVFXNiagaraBuilder::GetOrCreateVFXMaterial(
    const FString& BlendMode, const FString& TextureStyle,
    UTexture2D* Texture, const FString& OutputDir)
{
    /*
     * 간단한 VFX용 머티리얼 생성
     *
     * 프로덕션에서는 마스터 머티리얼 + 머티리얼 인스턴스 패턴이 더 적절.
     * 여기서는 각 에미터별로 MaterialInstanceDynamic 사용.
     *
     * 권장 구조:
     * - M_VFX_Additive (마스터 - Additive 블렌딩, 파티클 컬러 * 텍스처)
     * - M_VFX_Translucent (마스터 - Translucent 블렌딩, 알파 사용)
     * - MI_VFX_xxx (인스턴스 - 텍스처만 교체)
     */

    // 마스터 머티리얼 경로 (프로젝트에 미리 만들어둬야 함)
    FString MasterMaterialPath;
    if (BlendMode == TEXT("additive"))
    {
        MasterMaterialPath = TEXT("/AIVFXGenerator/Materials/M_VFX_Additive");
    }
    else if (BlendMode == TEXT("translucent"))
    {
        MasterMaterialPath = TEXT("/AIVFXGenerator/Materials/M_VFX_Translucent");
    }
    else
    {
        MasterMaterialPath = TEXT("/AIVFXGenerator/Materials/M_VFX_Opaque");
    }

    UMaterial* MasterMaterial = LoadObject<UMaterial>(nullptr, *MasterMaterialPath);

    if (MasterMaterial && Texture)
    {
        // MaterialInstanceDynamic 생성 (에디터에서는 MaterialInstanceConstant 권장)
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MasterMaterial, nullptr);
        MID->SetTextureParameterValue(FName("BaseTexture"), Texture);
        return MID;
    }

    // 마스터 머티리얼이 없으면 기본 파티클 머티리얼 반환
    UMaterialInterface* DefaultMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Niagara/DefaultAssets/DefaultSpriteMaterial"));
    return DefaultMat;
}

// ============================================================================
// Exposed Parameters 설정
// ============================================================================
void FVFXNiagaraBuilder::SetupExposedParameters(UNiagaraSystem* System,
    const TArray<FString>& ParameterNames)
{
    /*
     * User Exposed Parameters를 시스템에 추가
     * 런타임에 UNiagaraComponent::SetFloatParameter 등으로 제어 가능
     *
     * 기본 파라미터 세트:
     * - RadiusScale (float, 기본 1.0)
     * - IntensityMult (float, 기본 1.0)
     * - DurationScale (float, 기본 1.0)
     * - ElementTint (LinearColor, 기본 White)
     */

    FNiagaraUserRedirectionParameterStore& ParamStore =
        System->GetExposedParameters();

    for (const FString& ParamName : ParameterNames)
    {
        FNiagaraVariable Var;
        Var.SetName(FName(*ParamName));

        if (ParamName.Contains(TEXT("Color")) || ParamName.Contains(TEXT("Tint")))
        {
            Var.SetType(FNiagaraTypeDefinition::GetColorDef());
            ParamStore.AddParameter(Var, true);
            ParamStore.SetParameterValue<FLinearColor>(FLinearColor::White, Var);
        }
        else
        {
            Var.SetType(FNiagaraTypeDefinition::GetFloatDef());
            ParamStore.AddParameter(Var, true);
            ParamStore.SetParameterValue<float>(1.0f, Var);
        }
    }
}

// ============================================================================
// 파라미터 설정 유틸리티
// ============================================================================
void FVFXNiagaraBuilder::SetNiagaraVariableFloat(
    UNiagaraSystem* System, int32 EmitterIndex,
    const FString& ModuleName, const FString& ParamName, float Value)
{
    /*
     * Niagara 파라미터 설정의 실제 구현
     *
     * UE5 Niagara에서 에디터 스크립팅으로 파라미터를 설정하는 방법:
     *
     * 1. FNiagaraParameterStore를 통한 직접 접근
     * 2. UNiagaraScript의 RapidIterationParameters
     * 3. FNiagaraVariable 기반 접근
     *
     * 여기서는 RapidIterationParameters를 사용 (에디터 변경과 동일)
     */

    const auto& EmitterHandles = System->GetEmitterHandles();
    if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

    FVersionedNiagaraEmitterData* EmitterData =
        EmitterHandles[EmitterIndex].GetInstance().GetLatestEmitterData();
    if (!EmitterData) return;

    // RapidIterationParameters에서 매칭되는 변수 찾기
    FString FullParamName = FString::Printf(TEXT("%s.%s.%s"),
        *EmitterHandles[EmitterIndex].GetName().ToString(),
        *ModuleName, *ParamName);

    FNiagaraVariable Var;
    Var.SetName(FName(*FullParamName));
    Var.SetType(FNiagaraTypeDefinition::GetFloatDef());

    // 각 스크립트의 RapidIterationParameters에 설정
    for (UNiagaraScript* Script : {
        EmitterData->SpawnScriptProps.Script,
        EmitterData->UpdateScriptProps.Script })
    {
        if (Script)
        {
            Script->RapidIterationParameters.SetParameterValue<float>(Value, Var);
        }
    }
}

void FVFXNiagaraBuilder::SetNiagaraVariableVec3(
    UNiagaraSystem* System, int32 EmitterIndex,
    const FString& ModuleName, const FString& ParamName, FVector Value)
{
    const auto& EmitterHandles = System->GetEmitterHandles();
    if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

    FVersionedNiagaraEmitterData* EmitterData =
        EmitterHandles[EmitterIndex].GetInstance().GetLatestEmitterData();
    if (!EmitterData) return;

    FString FullParamName = FString::Printf(TEXT("%s.%s.%s"),
        *EmitterHandles[EmitterIndex].GetName().ToString(),
        *ModuleName, *ParamName);

    FNiagaraVariable Var;
    Var.SetName(FName(*FullParamName));
    Var.SetType(FNiagaraTypeDefinition::GetVec3Def());

    for (UNiagaraScript* Script : {
        EmitterData->SpawnScriptProps.Script,
        EmitterData->UpdateScriptProps.Script })
    {
        if (Script)
        {
            Script->RapidIterationParameters.SetParameterValue<FVector3f>(
                FVector3f(Value), Var);
        }
    }
}

void FVFXNiagaraBuilder::SetNiagaraVariableColor(
    UNiagaraSystem* System, int32 EmitterIndex,
    const FString& ModuleName, const FString& ParamName, FLinearColor Value)
{
    const auto& EmitterHandles = System->GetEmitterHandles();
    if (!EmitterHandles.IsValidIndex(EmitterIndex)) return;

    FVersionedNiagaraEmitterData* EmitterData =
        EmitterHandles[EmitterIndex].GetInstance().GetLatestEmitterData();
    if (!EmitterData) return;

    FString FullParamName = FString::Printf(TEXT("%s.%s.%s"),
        *EmitterHandles[EmitterIndex].GetName().ToString(),
        *ModuleName, *ParamName);

    FNiagaraVariable Var;
    Var.SetName(FName(*FullParamName));
    Var.SetType(FNiagaraTypeDefinition::GetColorDef());

    for (UNiagaraScript* Script : {
        EmitterData->SpawnScriptProps.Script,
        EmitterData->UpdateScriptProps.Script })
    {
        if (Script)
        {
            Script->RapidIterationParameters.SetParameterValue<FLinearColor>(Value, Var);
        }
    }
} 