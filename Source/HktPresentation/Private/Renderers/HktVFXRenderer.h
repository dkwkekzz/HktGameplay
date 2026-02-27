// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class ULocalPlayer;
class UHktVFXAssetBank;
class UNiagaraSystem;
class UNiagaraComponent;
struct FHktVFXIntent;
enum class EHktVFXElement : uint8;

/**
 * 이벤트 기반 VFX 재생기.
 * PresentationState Sync가 아닌, Intent 제출 등 즉시 이벤트에 의해 구동된다.
 *
 * 두 가지 에셋 로딩 경로:
 * - Tag 기반 (PlayVFXAtLocation): UHktAssetSubsystem → UHktVFXVisualDataAsset
 * - Intent 기반 (PlayVFXWithIntent): UHktVFXAssetBank → 퍼지 매칭
 */
class FHktVFXRenderer
{
public:
	explicit FHktVFXRenderer(ULocalPlayer* InLP);

	/** AssetBank 설정 (에디터 DataAsset) */
	void SetAssetBank(UHktVFXAssetBank* InBank);

	/** 폴백 Niagara 시스템 설정 (AssetBank 매칭 실패 시) */
	void SetFallbackSystem(UNiagaraSystem* InSystem);

	/** 월드 위치에 VFX 스폰 (일회성, 자동 소멸) */
	void PlayVFXAtLocation(FGameplayTag VFXTag, FVector Location);

	/** Intent 기반 VFX 스폰 (AssetBank 퍼지 매칭 + RuntimeOverrides) */
	void PlayVFXWithIntent(const FHktVFXIntent& Intent);

	void Teardown();

private:
	void ApplyRuntimeOverrides(UNiagaraComponent* Comp, const FHktVFXIntent& Intent);
	static FLinearColor GetElementTintColor(EHktVFXElement Element);

	ULocalPlayer* LocalPlayer = nullptr;
	UHktVFXAssetBank* AssetBank = nullptr;
	UNiagaraSystem* FallbackSystem = nullptr;
};
