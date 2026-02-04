// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HktPresentationGlobalSetting.generated.h"

class AHktCharacter;
class UMaterialInterface;
class UNiagaraSystem;
class UUserWidget;
class UTexture2D;

/**
 * HktPresentation 전역 설정
 * Project Settings -> Game -> Hkt Presentation Settings 에서 편집 가능
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Hkt Presentation Settings"))
class HKTPRESENTATION_API UHktPresentationGlobalSetting : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHktPresentationGlobalSetting();

	// Project Settings 메뉴 위치
	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Game"); }
	virtual FName GetSectionName() const override { return FName("HktPresentation"); }

	// === 엔티티 시각화 ===

	UPROPERTY(Config, EditAnywhere, Category = "Entity", meta = (DisplayName = "Default Character Class"))
	TSoftClassPtr<AHktCharacter> DefaultCharacterClass;

	// === 선택 시각화 ===

	UPROPERTY(Config, EditAnywhere, Category = "Selection", meta = (DisplayName = "Selection Material"))
	TSoftObjectPtr<UMaterialInterface> SelectionMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Selection", meta = (DisplayName = "Subject Selection Color"))
	FLinearColor SubjectSelectionColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Selection", meta = (DisplayName = "Target Selection Color"))
	FLinearColor TargetSelectionColor = FLinearColor(1.0f, 0.5f, 0.0f, 1.0f);

	// === 인터랙션 FX ===

	UPROPERTY(Config, EditAnywhere, Category = "Interaction", meta = (DisplayName = "Interaction FX System"))
	TSoftObjectPtr<UNiagaraSystem> InteractionFXSystem;

	// === HUD ===

	UPROPERTY(Config, EditAnywhere, Category = "HUD", meta = (DisplayName = "HUD Widget Class"))
	TSoftClassPtr<UUserWidget> HUDWidgetClass;

	// === 로그인 화면 ===

	/** 로그인 화면 배경 텍스처 (일러스트 등). 비어 있으면 기본 배경 사용 */
	UPROPERTY(Config, EditAnywhere, Category = "Login", meta = (DisplayName = "Login Background Texture"))
	TSoftObjectPtr<UTexture2D> LoginBackgroundTexture;
};
