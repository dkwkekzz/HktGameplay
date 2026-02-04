// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktTagDataAsset.h"
#include "HktWidgetLoginHudDataAsset.generated.h"

class UTexture2D;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;

/**
 * 로그인 위젯용 DataAsset.
 * Media Player / Texture / Source 경로를 가지며, HUD에서 비동기 로드 후 위젯 생성 시 전달합니다.
 */
UCLASS(BlueprintType)
class HKTPRESENTATION_API UHktWidgetLoginHudDataAsset : public UHktTagDataAsset
{
	GENERATED_BODY()

public:
    // TSoftObjectPtr 대신 포인터 직접 사용 (엔진이 알아서 같이 로드함)
    UPROPERTY(EditAnywhere, Category = "Login", meta = (DisplayName = "Login Widget"))
    TObjectPtr<UTexture2D> LoginBackgroundTexture;

    // 경로(Path) 대신 객체 포인터 사용
    UPROPERTY(EditAnywhere, Category = "Login Widget")
    TObjectPtr<UMediaPlayer> MediaPlayer;

    UPROPERTY(EditAnywhere, Category = "Login Widget")
    TObjectPtr<UMediaTexture> MediaTexture;

    UPROPERTY(EditAnywhere, Category = "Login Widget")
    TObjectPtr<UFileMediaSource> MediaSource;
};
