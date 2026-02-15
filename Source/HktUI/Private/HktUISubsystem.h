// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "TickableGameObject.h"
#include "IHktUIView.h"
#include "HktUISubsystem.generated.h"

class UHktUIElement;
class UHktUIAnchorStrategy;
class APlayerController;
class IHktPlayerInteractionInterface;

/**
 * HktUI 시스템의 백엔드 (LocalPlayerSubsystem).
 * 전체 UI 트리의 기술적 루트를 관리하고, 현재 PlayerController의 IHktPlayerInteractionInterface에 접근합니다.
 */
UCLASS()
class HKTUI_API UHktUISubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UHktUISubsystem* Get(APlayerController* PC);

	// --- USubsystem ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// --- ULocalPlayerSubsystem ---
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	// --- FTickableGameObject ---
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// --- IHktPlayerInteractionInterface 접근 ---
	IHktPlayerInteractionInterface* GetPlayerInteraction() const { return PlayerInteraction; }

	/** 뷰와 전략을 받아 Element를 생성하고 트리/캔버스에 등록 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	UHktUIElement* CreateElement(TSharedPtr<IHktUIView> InView, UHktUIAnchorStrategy* InStrategy, UHktUIElement* Parent = nullptr);

	/** Entity ID에 해당하는 Element를 반환. 없으면 새로 생성하여 등록 후 반환 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	UHktUIElement* GetOrAddEntityElement(int32 EntityID);

	/** Entity ID에 해당하는 Element를 제거 (맵에서만 제거, 객체 파괴는 호출측에서) */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	void RemoveEntityElement(int32 EntityID);

	/** Entity ID에 해당하는 Element를 찾아 반환 (없으면 nullptr) */
	UFUNCTION(BlueprintPure, Category = "Hkt|UI")
	UHktUIElement* FindEntityElement(int32 EntityID) const;

	/** Element의 View를 메인 캔버스에 추가 (CreateElement 시 자동 호출됨) */
	void AddElementToCanvas(UHktUIElement* Element);

private:
	void BindPlayerInteraction(APlayerController* PC);
	void UnbindPlayerInteraction();

	TWeakObjectPtr<APlayerController> CachedPlayerController;
	IHktPlayerInteractionInterface* PlayerInteraction = nullptr;

	UPROPERTY()
	TObjectPtr<UHktUIElement> RootElement;

	TSharedPtr<class SConstraintCanvas> MainCanvasWidget;

	UPROPERTY()
	TMap<int32, TObjectPtr<UHktUIElement>> EntityUIMap;

	bool bInitialized = false;

	void TickAllElements(float DeltaTime);
	void UpdateCanvasSlots();
};
