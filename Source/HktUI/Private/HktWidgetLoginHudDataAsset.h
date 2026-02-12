// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktTagDataAsset.h"
#include "HktWidgetLoginHudDataAsset.generated.h"

class UTexture2D;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;

UCLASS(BlueprintType)
class HKTUI_API UHktWidgetLoginHudDataAsset : public UHktTagDataAsset
{
	GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Login")
    TObjectPtr<UTexture2D> LoginBackgroundTexture;

    UPROPERTY(EditAnywhere, Category = "Login Widget")
    TObjectPtr<UMediaPlayer> MediaPlayer;

    UPROPERTY(EditAnywhere, Category = "Login Widget")
    TObjectPtr<UMediaTexture> MediaTexture;

    UPROPERTY(EditAnywhere, Category = "Login Widget")
    TObjectPtr<UFileMediaSource> MediaSource;
};
