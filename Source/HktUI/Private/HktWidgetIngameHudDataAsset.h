// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktUITagDataAsset.h"
#include "HktWidgetIngameHudDataAsset.generated.h"

/**
 * 인게임 뷰포트 HUD용 DataAsset.
 * CreateView()에서 SHktIngameHudWidget을 생성하여 반환합니다.
 */
UCLASS(BlueprintType)
class HKTUI_API UHktWidgetIngameHudDataAsset : public UHktUITagDataAsset
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<IHktUIView> CreateView() const override;
};
