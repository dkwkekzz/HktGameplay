// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktHUD.h"
#include "HktCoreDefs.h"
#include "HktWorldState.h"
#include "HktWorldView.h"
#include "HktIngameHUD.generated.h"

class UHktWorldViewAnchorStrategy;

/**
 * 인게임 맵 전용 HUD.
 * 뷰포트 UI (인벤토리/장착/스킬 버튼)와 엔티티 월드 HUD를 관리합니다.
 * GameMode의 HUDClass에 설정하여 사용합니다.
 */
UCLASS()
class HKTUI_API AHktIngameHUD : public AHktHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

protected:
	/** 인게임 뷰포트 위젯 태그 (기본값: Widget.IngameHud) */
	UPROPERTY(EditDefaultsOnly, Category = "Hkt|UI")
	FGameplayTag IngameWidgetTag;

	/** 엔티티 HUD 위젯 태그 (기본값: Widget.EntityHud) */
	UPROPERTY(EditDefaultsOnly, Category = "Hkt|UI")
	FGameplayTag EntityWidgetTag;

	/** 엔티티 HUD의 머리 위 오프셋 */
	UPROPERTY(EditDefaultsOnly, Category = "Hkt|UI")
	FVector EntityHudOffset = FVector(0.f, 0.f, 120.f);

	void UpdateEntityUI() override;

private:
	void OnWorldViewUpdated(const FHktWorldView& View);
	void RefreshWorldState();
	void SyncEntityElements();
	void CreateEntityElement(FHktEntityId EntityId);
	void UpdateEntityProperties();

	const FHktWorldState* CachedWorldState = nullptr;
	bool bWorldStateValid = false;
	bool bInitialSyncDone = false;

	TSet<FHktEntityId> TrackedEntities;
};
