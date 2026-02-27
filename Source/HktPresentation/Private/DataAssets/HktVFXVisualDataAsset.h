// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktTagDataAsset.h"
#include "HktVFXVisualDataAsset.generated.h"

class UNiagaraSystem;

/**
 * VFX 시각화용 TagDataAsset.
 * IdentifierTag(예: VFX.HitSpark)로 로드되며, 재생할 Niagara System을 지정합니다.
 */
UCLASS(BlueprintType)
class HKTPRESENTATION_API UHktVFXVisualDataAsset : public UHktTagDataAsset
{
	GENERATED_BODY()

public:
	/** 이 VFX 태그에 대응하는 Niagara System */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HKT|VFX")
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;
};
