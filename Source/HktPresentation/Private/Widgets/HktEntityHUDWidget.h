// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HktPresentationTypes.h"
#include "HktEntityHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * UHktEntityHUDWidget
 * 
 * 엔티티 머리 위에 표시되는 HUD 위젯
 * 체력바, 마나바, EntityId 표시
 */
UCLASS()
class HKTPRESENTATION_API UHktEntityHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Hkt|HUD")
	void UpdateHUDData(const FHktEntityHUDData& Data);

	UFUNCTION(BlueprintCallable, Category = "Hkt|HUD")
	void SetShowEntityId(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "Hkt|HUD")
	void SetShowHealthBar(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "Hkt|HUD")
	void SetShowManaBar(bool bShow);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Hkt|HUD")
	void OnHUDDataUpdated(const FHktEntityHUDData& Data);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Hkt|HUD")
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Hkt|HUD")
	TObjectPtr<UProgressBar> ManaBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Hkt|HUD")
	TObjectPtr<UTextBlock> EntityIdText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|HUD")
	bool bShowEntityId = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|HUD")
	bool bShowHealthBar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hkt|HUD")
	bool bShowManaBar = false;

private:
	FHktEntityHUDData CachedData;
};
