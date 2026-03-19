// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreDefs.h"

class ULocalPlayer;
class UHktVFXAssetBank;
class UNiagaraSystem;
class UNiagaraComponent;
struct FHktVFXIntent;
struct FHktPresentationState;
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

	/** 엔터티에 부착된 지속형 VFX 스폰 (선택 인디케이터 등) */
	void AttachVFXToEntity(FGameplayTag VFXTag, FHktEntityId EntityId, FVector Location);

	/** 엔터티에 부착된 VFX 제거 */
	void DetachVFXFromEntity(FGameplayTag VFXTag, FHktEntityId EntityId);

	/** 지속형 VFX 위치를 PresentationState 기반으로 업데이트 */
	void UpdateEntityVFXPositions(const FHktPresentationState& State);

	void Teardown();

private:
	void ApplyRuntimeOverrides(UNiagaraComponent* Comp, const FHktVFXIntent& Intent);
	static FLinearColor GetElementTintColor(EHktVFXElement Element);

	/** 엔터티 부착 VFX 키 (Tag + EntityId) */
	struct FEntityVFXKey
	{
		FGameplayTag Tag;
		FHktEntityId EntityId;
		bool operator==(const FEntityVFXKey& Other) const { return Tag == Other.Tag && EntityId == Other.EntityId; }
	};
	friend uint32 GetTypeHash(const FEntityVFXKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Tag), GetTypeHash(Key.EntityId));
	}

	ULocalPlayer* LocalPlayer = nullptr;
	UHktVFXAssetBank* AssetBank = nullptr;
	UNiagaraSystem* FallbackSystem = nullptr;

	/** 활성 엔터티 부착 VFX 컴포넌트 맵 */
	TMap<FEntityVFXKey, TWeakObjectPtr<UNiagaraComponent>> EntityVFXMap;
};
