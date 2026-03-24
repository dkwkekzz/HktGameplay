// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktHUD.h"
#include "HktCoreDefs.h"
#include "HktPresentationRenderer.h"
#include "HktPresentationState.h"
#include "HktIngameHUD.generated.h"

class UHktWidgetEntityHudDataAsset;
class UHktWorldViewAnchorStrategy;

/**
 * 인게임 맵 전용 HUD.
 * IHktPresentationRenderer를 구현하여 PresentationSubsystem으로부터 Sync를 수신합니다.
 * 카메라 이동 등 클라이언트 변경 시에도 엔티티 위젯 위치가 실시간 반영됩니다.
 */
UCLASS()
class HKTUI_API AHktIngameHUD : public AHktHUD, public IHktPresentationRenderer
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- IHktPresentationRenderer ---
	virtual void Sync(const FHktPresentationState& State) override;
	virtual void Teardown() override;

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

private:
	void SyncEntityElements(const FHktPresentationState& State);
	void CreateEntityElement(FHktEntityId EntityId, const FHktPresentationState& State);
	void UpdateEntityProperties(const FHktPresentationState& State);

	UPROPERTY()
	TObjectPtr<UHktWidgetEntityHudDataAsset> CachedEntityHudAsset;

	bool bInitialSyncDone = false;
	TSet<FHktEntityId> TrackedEntities;
};
