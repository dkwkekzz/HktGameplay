// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktTagDataAsset.h"
#include "IHktUIViewFactory.h"
#include "HktWidgetEntityHudDataAsset.generated.h"

class UHktUIAnchorStrategy;

/**
 * 엔티티 HUD용 DataAsset.
 * CreateView()에서 SHktEntityHudWidget을 생성하여 반환합니다.
 */
UCLASS(BlueprintType)
class HKTUI_API UHktWidgetEntityHudDataAsset : public UHktTagDataAsset, public IHktUIViewFactory
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Hkt|UI")
	TSubclassOf<UHktUIAnchorStrategy> DefaultAnchorStrategyClass;

	virtual TSharedPtr<IHktUIView> CreateView() const override;
	virtual UHktUIAnchorStrategy* CreateStrategy(UObject* Outer) const override;
};
