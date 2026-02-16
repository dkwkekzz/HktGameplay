// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktUITagDataAsset.h"
#include "HktWidgetEntityHudDataAsset.generated.h"

/**
 * 엔티티 HUD용 DataAsset.
 * CreateView()에서 SHktEntityHudWidget을 생성하여 반환합니다.
 */
UCLASS(BlueprintType)
class HKTUI_API UHktWidgetEntityHudDataAsset : public UHktUITagDataAsset
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<IHktUIView> CreateView() const override;
};
