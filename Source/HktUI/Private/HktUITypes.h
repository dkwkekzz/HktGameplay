// Copyright Hkt Studios, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HktCoreTypes.h"
#include "HktUITypes.generated.h"

// ============================================================================
// 위젯 부착 대상
// ============================================================================

/**
 * 위젯을 어디에 붙일 것인가.
 * 
 * Viewport: 화면 전체에 오버레이. 인자 불필요. (로그인, 인벤토리 등)
 * Widget:   기존 위젯의 자식으로 부착. TargetWidgetTag로 대상 지정.
 * Entity:   특정 HktEntity 위에 부착. IHktUserEventDispatcher로부터 위치 조회.
 *           Actor, MassEntity, 또는 Stash 위치 기반 모두 가능.
 */
UENUM(BlueprintType)
enum class EHktUIAttachTarget : uint8
{
	/** 뷰포트에 오버레이 (인자 없음) */
	Viewport,
	/** 기존 관리 위젯의 자식으로 부착 (TargetWidgetTag 필요) */
	Widget,
	/** HktEntity 위에 부착 (EntityId로 위치 조회) */
	Entity,
};

// ============================================================================
// UI 액션 타입
// ============================================================================

UENUM(BlueprintType)
enum class EHktUIActionType : uint8
{
	/** 위젯 생성 */
	CreateWidget,
	/** 위젯 파괴 */
	DestroyWidget,
	/** PlayerController에 이벤트 전달 */
	DispatchEvent,
	/** 위젯 생성 + 이벤트 전달 */
	CreateWidgetAndDispatch,
};

// ============================================================================
// HUD → Subsystem 이벤트
// ============================================================================

/**
 * HUD 위젯에서 발생한 이벤트
 */
USTRUCT(BlueprintType)
struct HKTUI_API FHktUIEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Hkt|UI")
	FGameplayTag EventTag;

	UPROPERTY(BlueprintReadWrite, Category = "Hkt|UI")
	TArray<FString> StringParams;

	UPROPERTY(BlueprintReadWrite, Category = "Hkt|UI")
	TArray<int32> IntParams;

	UPROPERTY(BlueprintReadWrite, Category = "Hkt|UI")
	FVector Location = FVector::ZeroVector;

	/** Entity 관련 이벤트일 때 */
	UPROPERTY(BlueprintReadWrite, Category = "Hkt|UI")
	int32 EntityId = -1;

	FHktUIEvent() = default;
	explicit FHktUIEvent(const FGameplayTag& InTag) : EventTag(InTag) {}
};

// ============================================================================
// 관리 위젯 엔트리 (Subsystem 내부)
// ============================================================================

/**
 * Subsystem이 관리하는 위젯 하나의 정보.
 * AttachTarget에 따라 Viewport, 부모 위젯, Entity 위치에 부착.
 */
USTRUCT()
struct FHktManagedWidgetEntry
{
	GENERATED_BODY()

	/** 위젯 인스턴스 */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Widget = nullptr;

	/** 부착 대상 */
	EHktUIAttachTarget AttachTarget = EHktUIAttachTarget::Viewport;

	/** Entity 부착 시 대상 EntityId */
	FHktEntityId EntityId = InvalidEntityId;

	/** Widget 부착 시 부모 위젯 태그 */
	FGameplayTag ParentWidgetTag;

	/** Entity 부착 시 WidgetComponent (Screen Space) */
	UPROPERTY(Transient)
	TObjectPtr<class UWidgetComponent> WidgetComponent = nullptr;

	bool IsValid() const { return Widget != nullptr; }
};
