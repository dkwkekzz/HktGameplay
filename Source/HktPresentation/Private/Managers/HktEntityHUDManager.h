// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HktPresentationTypes.h"

class UWorld;
class UUserWidget;
class UWidgetComponent;
class AHktCharacter;
class IHktStashInterface;
class UHktEntityHUDWidget;

/**
 * FHktEntityHUDManager
 * 
 * 엔티티 HUD 관리
 * - 체력바
 * - 마나바
 * - EntityId 표시 (디버그)
 */
class HKTPRESENTATION_API FHktEntityHUDManager
{
public:
	FHktEntityHUDManager(UWorld* InWorld);
	~FHktEntityHUDManager();

	// === 설정 ===
	
	void SetHUDOffset(FVector Offset) { HUDOffset = Offset; }
	void SetHUDDrawSize(FVector2D Size) { HUDDrawSize = Size; }

	// === HUD 관리 ===
	
	/** 엔티티에 HUD 추가 */
	void AddEntityHUD(FHktEntityId EntityId, AHktCharacter* Character);
	
	/** 엔티티 HUD 제거 */
	void RemoveEntityHUD(FHktEntityId EntityId);
	
	/** HUD 존재 여부 */
	bool HasEntityHUD(FHktEntityId EntityId) const;

	// === 업데이트 ===
	
	/** 단일 엔티티 HUD 데이터 업데이트 */
	void UpdateEntityHUD(FHktEntityId EntityId, const FHktEntityHUDData& Data);
	
	/** Stash에서 데이터 읽어 전체 업데이트 */
	void Tick(float DeltaTime, IHktStashInterface* Stash);

	// === 표시 설정 ===
	
	void SetShowEntityId(bool bShow);
	void SetShowHealthBar(bool bShow);
	void SetShowManaBar(bool bShow);

	// === 조회 ===
	
	UHktEntityHUDWidget* GetHUDWidget(FHktEntityId EntityId) const;
	int32 GetHUDCount() const { return EntityHUDMap.Num(); }

private:
	FHktEntityHUDData BuildHUDDataFromStash(FHktEntityId EntityId, IHktStashInterface* Stash) const;
	TSubclassOf<UUserWidget> GetHUDWidgetClass() const;

private:
	UWorld* World = nullptr;
	
	FVector HUDOffset = FVector(0.0f, 0.0f, 120.0f);
	FVector2D HUDDrawSize = FVector2D(100.0f, 30.0f);

	bool bShowEntityId = false;
	bool bShowHealthBar = true;
	bool bShowManaBar = false;

	struct FEntityHUDEntry
	{
		TWeakObjectPtr<UWidgetComponent> WidgetComponent;
		TWeakObjectPtr<UHktEntityHUDWidget> Widget;
	};
	
	TMap<int32, FEntityHUDEntry> EntityHUDMap;
};
