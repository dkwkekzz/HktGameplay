// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "HktHUD.generated.h"

class UHktUISubsystem;
class UHktUIElement;
class UHktUITagDataAsset;

/**
 * 뷰포트 UI의 진입점.
 * AssetSubsystem으로 UI DataAsset을 비동기 로드하고, 로드 완료 시 위젯을 생성합니다.
 */
UCLASS()
class HKTUI_API AHktHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	/**
	 * 태그에 해당하는 UI DataAsset을 비동기 로드한 뒤 CreateView/CreateStrategy로 Element 생성 및 등록.
	 * @param WidgetTag 로드할 위젯의 GameplayTag (HktAsset 태그 맵과 매핑)
	 * @param OnCreated 로드 및 생성 완료 시 호출되는 콜백 (nullptr 가능)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	void LoadAndCreateWidget(FGameplayTag WidgetTag, TFunction<void(UHktUIElement*)> OnCreated = nullptr);

	/** HktRuntime의 WorldView 참조 (설정 시 UpdateEntityUI에서 엔티티 UI 갱신) */
	UPROPERTY(BlueprintReadOnly, Category = "Hkt|UI")
	TSharedPtr<void> WorldView;

	/** 엔티티 UI 생성/제거/갱신 (WorldView가 설정된 경우에만 유효) */
	void UpdateEntityUI();

private:
	UPROPERTY()
	TObjectPtr<UHktUISubsystem> UISubsystem;
};
