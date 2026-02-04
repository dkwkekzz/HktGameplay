// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Slates/SHktLoginHudWidget.h"
#include "HktLoginHud.generated.h"

class UHktWidgetLoginHudDataAsset;

/**
 * 로그인 맵 전용 HUD.
 * Slate 로그인 위젯(ID/PW, 로그인 버튼)을 뷰포트에 표시하고, 배경은 설정에서 지정한 텍스처 사용.
 */
UCLASS()
class HKTPRESENTATION_API AHktLoginHud : public AHUD
{
	GENERATED_BODY()

public:
	AHktLoginHud();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void AddLoginWidgetToViewport();
	void CreateAndAddLoginWidget(const FOnHktLoginRequested& OnLogin, const TOptional<struct FSlateBrush>& BackgroundBrush, UHktWidgetLoginHudDataAsset* LoginWidgetDataAsset);
	void RemoveLoginWidgetFromViewport();

	TSharedPtr<class SWidget> LoginWidgetSlate;
};
