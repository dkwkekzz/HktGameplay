// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "HktUITagDataAsset.h"
#include "HktWidgetLoginHudDataAsset.generated.h"

class UTexture2D;

/**
 * 로그인 HUD용 DataAsset 예시.
 * CreateView()에서 Slate 위젯을 생성할 때 사용하는 리소스를 정의합니다.
 */
UCLASS(BlueprintType)
class HKTUI_API UHktWidgetLoginHudDataAsset : public UHktUITagDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Hkt|Login")
	TObjectPtr<UTexture2D> LoginBackgroundTexture;

	/** 구체적인 Slate 위젯 생성. SHktLoginHudWidget 구현 후 해당 위젯을 반환하도록 오버라이드 가능 */
	virtual TSharedPtr<IHktUIView> CreateView() const override;
};
