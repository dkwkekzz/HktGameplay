// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VFXNiagaraConfig.h"

class UNiagaraSystem;
class UTexture2D;
class UMaterialInterface;

/**
 * LLM이 준 FVFXNiagaraConfig + 텍스처 맵으로 UNiagaraSystem .uasset 생성.
 * 에디터 전용.
 */
class HKTVFXEDITOR_API FVFXNiagaraBuilder
{
public:
	/**
	 * Config와 에미터별 텍스처로 Niagara 시스템 에셋 빌드 및 디스크 저장.
	 * @return 생성된 UNiagaraSystem, 실패 시 nullptr
	 */
	UNiagaraSystem* BuildNiagaraSystem(
		const FVFXNiagaraConfig& Config,
		const TMap<FString, UTexture2D*>& Textures,
		const FString& OutputDirectory);

private:
	void ConfigureEmitter(UNiagaraSystem* System, int32 EmitterIndex,
		const FVFXEmitterConfig& Config, UTexture2D* Texture);
	void SetupSpawnModule(UNiagaraSystem* System, int32 EmitterIndex, const FVFXEmitterSpawnConfig& Config);
	void SetupInitializeModule(UNiagaraSystem* System, int32 EmitterIndex, const FVFXEmitterInitConfig& Config);
	void SetupUpdateModules(UNiagaraSystem* System, int32 EmitterIndex, const FVFXEmitterUpdateConfig& Config);
	void SetupRenderer(UNiagaraSystem* System, int32 EmitterIndex, const FVFXEmitterRenderConfig& Config, UTexture2D* Texture);
	UMaterialInterface* GetOrCreateVFXMaterial(const FString& BlendMode, const FString& TextureStyle,
		UTexture2D* Texture, const FString& OutputDir);
	void SetupExposedParameters(UNiagaraSystem* System, const TArray<FString>& ParameterNames);

	void SetNiagaraVariableFloat(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, float Value);
	void SetNiagaraVariableVec3(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FVector Value);
	void SetNiagaraVariableColor(UNiagaraSystem* System, int32 EmitterIndex,
		const FString& ModuleName, const FString& ParamName, FLinearColor Value);
};
