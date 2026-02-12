// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "GameplayTagContainer.h"
#include "HktUITypes.h"
#include "HktUISubsystem.generated.h"

// Forward declarations
class IHktUserEventDispatcher;
class IHktStashInterface;
class APlayerController;
class UHktEventParam;
class UHktUIActionDataAsset;
class UUserWidget;
class UWidgetComponent;

/**
 * UHktUISubsystem
 *
 * HktUI의 메인 LocalPlayerSubsystem.
 * 
 * 위젯 부착 추상화:
 *   Viewport → AddToViewport (인자 없음)
 *   Widget   → 기존 관리 위젯의 자식으로 부착 (ParentWidgetTag)
 *   Entity   → IHktUserEventDispatcher::GetEntityLocationInfo로 위치 조회
 *              WidgetComponent(Screen Space)를 직접 생성하여 월드 위치에 배치
 *              Actor/MassEntity/대상 없음 모두 지원
 *
 * Entity 위젯의 Tick:
 *   매 프레임 IHktUserEventDispatcher에서 위치를 조회하여 WidgetComponent 갱신.
 *   엔티티가 사라지면(bIsValid=false) 자동 제거.
 */
UCLASS()
class HKTUI_API UHktUISubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UHktUISubsystem();
	~UHktUISubsystem();

	static UHktUISubsystem* Get(APlayerController* PC);

	// === USubsystem Interface ===
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void BeginDestroy() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// === ULocalPlayerSubsystem ===
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	// === FTickableGameObject Interface ===
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// =========================================================================
	// UI 이벤트 (HUD 위젯 → Subsystem)
	// =========================================================================

	/** HUD 위젯에서 발생한 이벤트. 태그로 DataAsset 로드 후 액션 수행. */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	void HandleUIEvent(const FHktUIEvent& Event);

	/** DataAsset 없이 직접 PlayerController에 이벤트 전달 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	void DispatchToPlayerController(const FGameplayTag& EventTag, UHktEventParam* Param);

	// =========================================================================
	// 위젯 관리 (범용)
	// =========================================================================

	/**
	 * 위젯 생성.
	 * @param WidgetClass    생성할 위젯 클래스
	 * @param WidgetTag      관리용 태그 (조회/파괴에 사용)
	 * @param AttachTarget   부착 대상
	 * @param EntityId       Entity 부착 시 대상 (AttachTarget::Entity일 때만)
	 * @param ParentWidgetTag Widget 부착 시 부모 태그 (AttachTarget::Widget일 때만)
	 * @param AttachOffset   Entity 부착 시 오프셋
	 * @param DrawSize       Entity 부착 시 WidgetComponent DrawSize
	 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	UUserWidget* CreateManagedWidget(
		TSubclassOf<UUserWidget> WidgetClass,
		const FGameplayTag& WidgetTag,
		EHktUIAttachTarget AttachTarget = EHktUIAttachTarget::Viewport,
		FHktEntityId EntityId = InvalidEntityId,
		FGameplayTag ParentWidgetTag = FGameplayTag(),
		FVector AttachOffset = FVector(0.0f, 0.0f, 120.0f),
		FVector2D DrawSize = FVector2D(100.0f, 30.0f));

	/** 태그로 위젯 파괴 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	void DestroyManagedWidget(const FGameplayTag& WidgetTag);

	/** 특정 EntityId에 부착된 모든 위젯 파괴 */
	UFUNCTION(BlueprintCallable, Category = "Hkt|UI")
	void DestroyEntityWidgets(FHktEntityId EntityId);

	/** 태그로 위젯 조회 */
	UFUNCTION(BlueprintPure, Category = "Hkt|UI")
	UUserWidget* GetManagedWidget(const FGameplayTag& WidgetTag) const;

	// =========================================================================
	// Dispatcher 접근
	// =========================================================================

	IHktUserEventDispatcher* GetDispatcher() const { return Dispatcher; }

protected:
	void BindDispatcher(APlayerController* PC);
	void UnbindDispatcher();

	// === 엔티티 이벤트 핸들러 ===
	void HandleEntityCreated(FHktEntityId EntityId);
	void HandleEntityDestroyed(FHktEntityId EntityId);

	// === DataAsset 기반 액션 ===
	void ExecuteUIAction(UHktUIActionDataAsset* ActionAsset, const FHktUIEvent& Event);

	// === Entity 위젯 Tick ===
	void TickEntityWidgets();

	// === 내부 위젯 부착 ===
	UUserWidget* AttachToViewport(TSubclassOf<UUserWidget> WidgetClass);
	UUserWidget* AttachToWidget(TSubclassOf<UUserWidget> WidgetClass, const FGameplayTag& ParentTag);
	UWidgetComponent* AttachToEntity(TSubclassOf<UUserWidget> WidgetClass, FHktEntityId EntityId,
		const FVector& Offset, const FVector2D& DrawSize);

private:
	TWeakObjectPtr<APlayerController> CachedPlayerController;
	IHktUserEventDispatcher* Dispatcher = nullptr;

	/** Tag → ManagedWidgetEntry */
	UPROPERTY(Transient)
	TMap<FGameplayTag, FHktManagedWidgetEntry> ManagedWidgets;

	FDelegateHandle EntityCreatedHandle;
	FDelegateHandle EntityDestroyedHandle;

	bool bIsBound = false;
	bool bInitialized = false;
};
